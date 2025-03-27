#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include "mapa.h"


/*
Decidimos para esse projeto simular uma fila de instruções de cada thread para fins didáticos,
Cada entidade terá sua estrutura que será utilizada para a comunicação entre threads.
Seria possível disponibilizamos as variáveis das threads entre si com Casting, porém é menos divertido.
*/

/*
Todo:
- Algoritmo de movimento aleatório dos taxis
- Serialização da rota CO x View
- BFS e resolução de colisões
- Render 
- Alterar MOVE_TO para ser uma request para View
- Correção e padronização do Uso da mensagem
- Elaboração da matriz de visualização final (Com ascii e infos)
- Criação de Passageiros em loc random
- Synch All

Outras coisas que ainda nn consideramos
*/


//-------------------- ENTIDADES PRINCIPAIS ------------------------ 

// ---------- DEFINIÇÕES DA FILA DE COMANDOS COMPARTILHADA --------------

typedef enum {
    STATUS_REQUEST,   
    STATUS_UPDATE,    
    START_ROUTE,      
    MOVE_TO,          
    ACKNOWLEDGE,
    PATHFIND_REQUEST,
    PATHFIND_RESPONSE,
    EXIT       
} MessageType;

// Node
typedef struct Message {
    MessageType type;
    int sender_id;   
    int target_id;   
    int data_x;      
    int data_y;
    int extra_x;     
    int extra_y;
    int* pathX;
    int* pathY;
    int pathLen;
    struct Message* next;
} Message;

// ---------- DEFINIÇÕES DA FILA DE COMANDOS COMPARTILHADA --------------

// Fila
typedef struct {
    Message* head;
    Message* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} MessageQueue;

// ----------- DEFINIÇÕES DO TAXI -------------------

// Definição dos Taxis
typedef struct {
    int id;
    int x, y;
    bool isFree;
    int currentPassenger;
    MessageQueue queue;  
    pthread_mutex_t lock;
    ControlCenter* control_center;  // Todo taxi terá visão do seu centro de controle, dessa maneira cada um deles pode interagir na fila
} Taxi;


// ----------- DEFINIÇÕES DO VISUALIZADOR -------------------------
typedef struct {
    int numQuadrados;
    int larguraRua;
    int larguraBorda;
    int minTamanho;
    int maxTamanho;
    int distanciaMinima;
    ControlCenter* center; // Added field for ControlCenter
    MessageQueue queue;
    pthread_mutex_t lock; 
} InitVisualizer;

// ----------- DEFINIÇÕES DO CENTRO DE CONTROLE -------------------------

typedef struct {
    Taxi* taxis;
    int numTaxis;
    Passenger* passengers;
    int numPassengers;
    MessageQueue queue;
    MessageQueue view_queue;
    pthread_mutex_t lock;
} ControlCenter;


void* visualizador_thread(void* arg) {

    InitVisualizer* center = (InitVisualizer*)arg;

    // Cria o mapa
    Mapa* mapa = criarMapa();
    if (!mapa) return 1;

    // Gera o mapa com os paremetros enviados pelo Control Center
    gerarMapa(mapa, center->numQuadrados, center->larguraRua, center->larguraBorda, center->minTamanho, center->maxTamanho, center->distanciaMinima);

    while (1) {
        Message* msg = dequeue_message(&center->queue);
        
        pthread_mutex_lock(&center->lock);
        
        switch (msg->type) {
            case PATHFIND_REQUEST:

            int max_path = mapa->linhas * mapa->colunas;
            int* path_x = malloc(sizeof(int) * max_path);
            int* path_y = malloc(sizeof(int) * max_path);
            int path_len = 0;

            // Implementar algoritmo de busca de caminho
            if(encontraCaminhoMapa(mapa, msg->data_x, msg->data_y, msg->extra_x, msg->extra_y, path_x, path_y, path_len)){

                // Enviar PATHFIND_RESPONSE com a rota
                enqueue_message(&center->center->queue, PATHFIND_RESPONSE, msg->target_id, msg->sender_id, 0, 0, 0, 0, path_x, path_y, path_len);
            
            }else{

                enqueue_message(&center->center->queue, PATHFIND_RESPONSE, msg->target_id, msg->sender_id, 0, 0, 0, 0, NULL, NULL, NULL);
                free(path_y);  
                free(path_x);  
            }
            break;

        }
        
        if(msg->type == EXIT) {
            pthread_mutex_unlock(&center->lock);
            free(msg);
            break;
        }

        pthread_mutex_unlock(&center->lock);
        free(msg);

    }

    // Libera a memória
    desalocarMapa(mapa);

}


void* taxi_thread(void* arg) {
    Taxi* taxi = (Taxi*)arg; 

    while (1) {
        Message* msg = dequeue_message(&taxi->queue); 

        pthread_mutex_lock(&taxi->lock);
        
        switch (msg->type) {
            case STATUS_REQUEST:
                enqueue_message(taxi->control_center->queue, STATUS_UPDATE, taxi->id, 0, taxi->x, taxi->y, taxi->isFree, 0,0,0,0);
                break;

            case START_ROUTE:
                taxi->isFree = false;
                printf("Taxi %d received order to start route.\n", taxi->id);
                break;

            case MOVE_TO:
                taxi->x = msg->data_x;
                taxi->y = msg->data_y;
                printf("Taxi %d moving to (%d, %d)\n", taxi->id, taxi->x, taxi->y);
                break;
     
        }

        pthread_mutex_unlock(&taxi->lock); 
        free(msg);
        sleep(1);
    }
}




void assign_taxi_to_passenger(ControlCenter* center, int taxi_id, int passenger_x, int passenger_y) {
    Taxi* taxi = &center->taxis[taxi_id];

    // Send a PATHFIND_REQUEST message to the View Queue
    enqueue_message(&center->view_queue, PATHFIND_REQUEST, taxi->x, taxi->y, passenger_x, passenger_y, 0, 0,0,0,0);

    // Wait for the PATHFIND_RESPONSE with the route
    Message* msg = dequeue_message(&center->view_queue);
    if (msg->type == PATHFIND_RESPONSE && msg->pathX != NULL) {

        int* route_x = msg->pathX;  
        int* route_y = msg->pathY;  
        int route_length = msg->pathLen;  

        for (int i = 0; i < route_length; i++) {
            enqueue_message(&taxi->queue, MOVE_TO, 0, taxi_id, 0, 0, route_x[i], route_y[i],0,0,0);
            sleep(1);
        }

        printf("Taxi %d assigned route to passenger location (%d, %d)\n", taxi_id, passenger_x, passenger_y);
    }

    

    free(msg);
}

// Implementar geração automática aqui no x, y
void create_passenger(ControlCenter* center, int x, int y) {
    pthread_mutex_lock(&center->lock);

    // Allocate memory for the new passenger
    center->passengers = realloc(center->passengers, (center->numPassengers + 1) * sizeof(Passenger));
    center->passengers[center->numPassengers].id = center->numPassengers + 1;
    center->passengers[center->numPassengers].x = x;
    center->passengers[center->numPassengers].y = y;
    center->passengers[center->numPassengers].status = WAITING;

    center->numPassengers++;

    pthread_mutex_unlock(&center->lock);
}

// ---------- DEFINIÇÕES PASSAGEIROS -----------------------------------

typedef enum {
    WAITING,
    ASSIGNED,
    IN_RIDE
} PassengerState;

typedef struct {
    int id;
    int x, y;
    PassengerState state;
} Passenger;

void init_queue(MessageQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Funções de fila: enqueue / dequeue
void enqueue_message(MessageQueue* queue, MessageType type, int sender_id, int target_id, int x, int y, int extra_x, int extra_y, int* pathX, int* pathY, int pathLen) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->sender_id = sender_id;
    new_msg->target_id = target_id;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
    new_msg->next = NULL;
    new_msg->pathX = pathX;
    new_msg->pathY = pathY;
    new_msg->pathLen = pathLen;

    pthread_mutex_lock(&queue->lock);
    if (queue->tail == NULL) {
        queue->head = queue->tail = new_msg;
    } else {
        queue->tail->next = new_msg;
        queue->tail = new_msg;
    }
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

Message* dequeue_message(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    Message* msg = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) queue->tail = NULL;

    pthread_mutex_unlock(&queue->lock);
    return msg;  
}

// --------------------- FUNÇÕES DE GAME --------------------------------


/*
Readaptar função para receber mapa de jogo, e as posições iniciais dos taxis serem ruas
*/
// Função de inicialização, cria centro de controle e taxis 
//(precisa inicializar o vizualizador aqui ja com os parametros de entrada do mapa)
void init_operations(ControlCenter* center, int numTaxis) {
    center->numTaxis = numTaxis;
    center->taxis = malloc(numTaxis * sizeof(Taxi));
    pthread_mutex_init(&center->lock, NULL);
    
    // Initialize the message queues
    init_queue(&center->queue);      // Control Center's queue
    init_queue(&center->view_queue); // Queue for View communication
    
    InitVisualizer* initVisualizer = malloc(sizeof(InitVisualizer));
    initVisualizer->center = center;
    pthread_t visualizador_thread_id;

    initVisualizer->numQuadrados = 15;
    initVisualizer->larguraRua = 2;
    initVisualizer->larguraBorda = 2;
    initVisualizer->minTamanho = 10;
    initVisualizer->maxTamanho = 12;
    initVisualizer->distanciaMinima = 8;

    pthread_create(&visualizador_thread_id, NULL, visualizador_thread, initVisualizer);



    pthread_t taxiThreads[numTaxis];

    for (int i = 0; i < numTaxis; i++) {
        center->taxis[i].id = i + 1;
        center->taxis[i].currentPassenger = -1;
        center->taxis[i].isFree = true;
        init_queue(&center->taxis[i].queue);  
        pthread_mutex_init(&center->taxis[i].lock, NULL);
        center->taxis[i].control_center = center;  // Link taxi to the control center

        pthread_create(&taxiThreads[i], NULL, taxi_thread, &center->taxis[i]);
    }
}




int main() {
    ControlCenter center;
    int numTaxis = 3;  
    init_operations(&center, numTaxis);

    while (1) {
        sleep(5);
        request_status_from_taxis();
    }

    return 0;
}

