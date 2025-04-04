#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/ioctl.h>

// -------------------- CONSTANTS --------------------

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_TENTATIVAS 1000

#define CAMINHO_LIVRE '0'
#define ORIGEM 'E'
#define DESTINO 'S'
#define VISITADO '-'
#define RUA '0'
#define CALCADA '1'
#define DIREITA '>'
#define ESQUERDA '<'
#define CIMA '^'
#define BAIXO 'v'
#define TAXI 'T'
#define PASSENGER 'P'

#define CALCADA_EMOJI "⬛"  
#define RUA_EMOJI "⬜"       
#define PASSENGER_EMOJI "🙋" 
#define DESTINO_EMOJI "🏠"   
#define TAXI_EMOJI "🚖"

#define MAP_VERTICAL_PROPORTION 0.5
#define MAP_HORIZONTAL_PROPORTION 0.4

#define MAX_TAXIS 10

// -------------------- STRUCTURES --------------------

// Node for BFS
typedef struct {
    int x;
    int y;
    int parent_index;
} Node;

// Quadrado (block) structure
typedef struct {
    int x, y;
    int tamanho;
} Quadrado;

// Mapa structure
typedef struct {
    int linhas, colunas;
    int largura_rua;
    char **matriz;
} Mapa;

// Message types
typedef enum {
    CREATE_PASSENGER,
    RESET_MAP,
    EXIT_PROGRAM,
    PATHFIND_REQUEST,
    RANDOM_REQUEST,
    ROUTE_PLAN,
    EXIT,
    STATUS_REQUEST,
    CREATE_TAXI,
    DESTROY_TAXI,
    SPAWN_TAXI,
    MOVE_TO,
    FINISH
} MessageType;

// Message structure
typedef struct Message {
    MessageType type;
    struct Message* next;
    int data_x;
    int data_y;
    int extra_x;
    int extra_y;
    void* pointer;
} Message;

// Message queue structure
typedef struct {
    Message* head;
    Message* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} MessageQueue;

typedef struct {
    int* solucaoX;         
    int* solucaoY;         
    int tamanho_solucao;   
} PathData;

// Taxi structure
typedef struct {
    int id;
    int x, y;
    bool isFree;
    int currentPassenger;
    MessageQueue queue;  
    pthread_mutex_t lock;
    MessageQueue* control_queue;
    MessageQueue* visualizerQueue; 
    pthread_t thread_id; 
} Taxi;

// Control center structure
typedef struct {
    int numPassengers;
    pthread_mutex_t lock;
    MessageQueue queue;
    MessageQueue* visualizerQueue; // Added visualizer queue reference
    Taxi* taxis[MAX_TAXIS];
    int numTaxis;
} ControlCenter;


// Visualizer structure
typedef struct {
    int numQuadrados;
    int larguraRua;
    int larguraBorda;
    int minTamanho;
    int maxTamanho;
    int distanciaMinima;
    MessageQueue queue;
    MessageQueue* control_queue;
} Visualizer;



// Function prototypes
static void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda);
static void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados);
static void encontrarPontosConexao(Quadrado a, Quadrado b, int *px1, int *py1, int *px2, int *py2);
void init_operations();
void renderMap(Mapa* mapa);
pthread_t create_taxi_thread(Taxi* taxi);
bool find_random_free_point(Mapa* mapa, int* random_x, int* random_y);

// -------------------- QUEUE FUNCTIONS --------------------

// Initialize the queue
void init_queue(MessageQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Enqueue a message
void enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
    new_msg->pointer = pointer;
    new_msg->next = NULL;

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

// Priority enqueue a message
void priority_enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
    new_msg->pointer = pointer;
    new_msg->next = NULL;

    pthread_mutex_lock(&queue->lock);

    // Insert the message at the head of the queue
    if (queue->head == NULL) {
        queue->head = queue->tail = new_msg;
    } else {
        new_msg->next = queue->head;
        queue->head = new_msg;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

// Dequeue a message
Message* dequeue_message(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    Message* msg = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->lock);
    return msg;
}

// Cleanup the queue
void cleanup_queue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock); // Lock the queue to ensure thread safety

    Message* current = queue->head;
    while (current != NULL) {
        Message* next = current->next; // Save the next message
        free(current);                 // Free the current message
        current = next;                // Move to the next message
    }

    // Reset the queue
    queue->head = NULL;
    queue->tail = NULL;

    pthread_mutex_unlock(&queue->lock); // Unlock the queue
}

// -------------------- MAP FUNCTIONS --------------------

// Create a new map
Mapa* criarMapa() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("Error getting terminal size");
        return NULL;
    }

    // Scale the terminal size by 0.6
    int scaled_rows = (int)(ws.ws_row * MAP_VERTICAL_PROPORTION);
    int scaled_cols = (int)(ws.ws_col * MAP_HORIZONTAL_PROPORTION);

    Mapa* mapa = malloc(sizeof(Mapa));
    mapa->linhas = scaled_rows;
    mapa->colunas = scaled_cols;
    mapa->largura_rua = 1;

    mapa->matriz = malloc(mapa->linhas * sizeof(char*));
    for (int i = 0; i < mapa->linhas; i++) {
        mapa->matriz[i] = malloc(mapa->colunas * sizeof(char));
        for (int j = 0; j < mapa->colunas; j++) {
            mapa->matriz[i][j] = CALCADA;
        }
    }

    return mapa;
}

// Free the map
void desalocarMapa(Mapa* mapa) {
    if (!mapa) return;

    for (int i = 0; i < mapa->linhas; i++) {
        free(mapa->matriz[i]);
    }
    free(mapa->matriz);
    free(mapa);
}

// Generate the map
void gerarMapa(Mapa* mapa, int num_quadrados, int largura_rua, int largura_borda, int min_tamanho, int max_tamanho, int distancia_minima) {
    mapa->largura_rua = largura_rua;
    srand(time(NULL));

    if (min_tamanho <= 0 || max_tamanho < min_tamanho || mapa->linhas <= 0 || mapa->colunas <= 0) {
        return;
    }

    int tamanho_max_possivel = MIN(mapa->colunas, mapa->linhas) - 2 * largura_borda;
    if (max_tamanho > tamanho_max_possivel) {
        max_tamanho = tamanho_max_possivel;
        min_tamanho = MIN(min_tamanho, max_tamanho);
    }

    Quadrado quadrados[num_quadrados];
    int count = 0, tentativas = 0;

    // Clear the map (initialize with sidewalks)
    for (int i = 0; i < mapa->linhas; i++) {
        for (int j = 0; j < mapa->colunas; j++) {
            mapa->matriz[i][j] = CALCADA;
        }
    }

    // Generate squares (blocks)
    while (count < num_quadrados && tentativas < MAX_TENTATIVAS * num_quadrados) {
        int tamanho = rand() % (max_tamanho - min_tamanho + 1) + min_tamanho;

        int max_col = mapa->colunas - tamanho - largura_borda;
        int max_lin = mapa->linhas - tamanho - largura_borda;

        int dist_atual = distancia_minima;
        if (max_col <= 0 || max_lin <= 0) {
            dist_atual = 0;
            max_col = mapa->colunas - tamanho;
            max_lin = mapa->linhas - tamanho;
            if (max_col <= 0 || max_lin <= 0) break;
        }

        Quadrado q = {
            .x = rand() % (max_col + 1),
            .y = rand() % (max_lin + 1),
            .tamanho = tamanho
        };

        bool valido = true;
        for (int i = 0; i < count; i++) {
            int dx = abs((q.x + q.tamanho / 2) - (quadrados[i].x + quadrados[i].tamanho / 2));
            int dy = abs((q.y + q.tamanho / 2) - (quadrados[i].y + quadrados[i].tamanho / 2));
            if (dx < dist_atual && dy < dist_atual) {
                valido = false;
                break;
            }
        }

        if (valido) {
            quadrados[count++] = q;
            desenharQuadrado(mapa, q, largura_borda);
            tentativas = 0;
        } else {
            tentativas++;
        }
    }

    // Connect squares using MST logic
    if (count > 0) {
        conectarQuadradosMST(mapa, quadrados, count);
    }
}

// Print the map
void imprimirMapaLogico(Mapa* mapa) {
    for (int i = 0; i < mapa->linhas; i++) {
        for (int j = 0; j < mapa->colunas; j++) {
            printf("%c", mapa->matriz[i][j]);
        }
        printf("\n");
    }
}

// Render the map with emojis
void renderMap(Mapa* mapa) {
    if (!mapa || !mapa->matriz) {
        printf("Error: Map is not initialized.\n");
        return;
    }

    for (int i = 0; i < mapa->linhas; i++) {
        for (int j = 0; j < mapa->colunas; j++) {
            switch (mapa->matriz[i][j]) {
                case CALCADA:
                    printf(CALCADA_EMOJI);
                    break;
                case RUA:
                    printf(RUA_EMOJI);
                    break;
                case DESTINO:
                    printf(DESTINO_EMOJI);
                    break;
                case PASSENGER:
                    printf(PASSENGER_EMOJI);
                    break;
                case TAXI:
                    printf(TAXI_EMOJI);
                    break;
                default:
                    printf("?"); // Empty space
                    break;
            }
        }
        printf("\n");
    }
}

// Find path in the map
int encontraCaminho(int col_inicio, int lin_inicio, char **maze, int num_colunas, int num_linhas,
                    int solucaoCol[], int solucaoLin[], int *tamanho_solucao) {
    // BFS queue
    Node *fila = malloc(num_colunas * num_linhas * sizeof(Node));
    int inicio = 0, fim = 0;

    // Visited matrix
    int **visitado = malloc(num_linhas * sizeof(int *));
    for (int lin = 0; lin < num_linhas; lin++) {
        visitado[lin] = malloc(num_colunas * sizeof(int));
        for (int col = 0; col < num_colunas; col++) {
            visitado[lin][col] = -1;
        }
    }

    // Add the starting node
    fila[fim++] = (Node){.x = col_inicio, .y = lin_inicio, .parent_index = -1};
    visitado[lin_inicio][col_inicio] = 0;

    // Directions for moving (left, right, up, down)
    int delta_col[] = {0, 0, -1, 1};
    int delta_lin[] = {-1, 1, 0, 0};

    // BFS loop
    while (inicio < fim) {
        Node atual = fila[inicio++];

        // Check if it's the destination
        if (maze[atual.y][atual.x] == DESTINO) {
            // Count the path length
            int contador = 0;
            int index = inicio - 1;
            while (index != -1) {
                contador++;
                index = fila[index].parent_index;
            }

            // Fill the path in the correct order (start -> destination)
            *tamanho_solucao = contador;
            index = inicio - 1;
            for (int i = contador - 1; i >= 0; i--) {
                solucaoCol[i] = fila[index].x;
                solucaoLin[i] = fila[index].y;
                index = fila[index].parent_index;
            }

            // Free memory
            for (int lin = 0; lin < num_linhas; lin++) free(visitado[lin]);
            free(visitado);
            free(fila);
            return 0;
        }

        // Explore neighbors
        for (int i = 0; i < 4; i++) {
            int nova_col = atual.x + delta_col[i];
            int nova_lin = atual.y + delta_lin[i];

            // Check bounds
            if (nova_col < 0 || nova_col >= num_colunas || nova_lin < 0 || nova_lin >= num_linhas) {
                continue;
            }

            // Check if it's a valid path and not visited
            if ((maze[nova_lin][nova_col] == CAMINHO_LIVRE || maze[nova_lin][nova_col] == DESTINO) &&
                visitado[nova_lin][nova_col] == -1) {
                visitado[nova_lin][nova_col] = inicio - 1;
                fila[fim++] = (Node){.x = nova_col, .y = nova_lin, .parent_index = inicio - 1};
            }
        }
    }

    // Free memory on failure
    for (int lin = 0; lin < num_linhas; lin++) free(visitado[lin]);
    free(visitado);
    free(fila);
    return 1;
}

// Find path with specific coordinates
int encontraCaminhoCoordenadas(int col_inicio, int lin_inicio, int col_destino, int lin_destino,
                               char **maze, int num_colunas, int num_linhas,
                               int solucaoCol[], int solucaoLin[], int *tamanho_solucao) {
    // BFS queue
    Node *fila = malloc(num_colunas * num_linhas * sizeof(Node));
    int inicio = 0, fim = 0;

    // Visited matrix
    int **visitado = malloc(num_linhas * sizeof(int *));
    for (int lin = 0; lin < num_linhas; lin++) {
        visitado[lin] = malloc(num_colunas * sizeof(int));
        for (int col = 0; col < num_colunas; col++) {
            visitado[lin][col] = -1;
        }
    }

    // Add the starting node
    fila[fim++] = (Node){.x = col_inicio, .y = lin_inicio, .parent_index = -1};
    visitado[lin_inicio][col_inicio] = 0;

    // Directions for moving (left, right, up, down)
    int delta_col[] = {0, 0, -1, 1};
    int delta_lin[] = {-1, 1, 0, 0};

    // BFS loop
    while (inicio < fim) {
        Node atual = fila[inicio++];

        // Check if it's the destination
        if (atual.x == col_destino && atual.y == lin_destino) {
            // Count the path length
            int contador = 0;
            int index = inicio - 1;
            while (index != -1) {
                contador++;
                index = fila[index].parent_index;
            }

            // Fill the path in the correct order (start -> destination)
            *tamanho_solucao = contador;
            index = inicio - 1;
            for (int i = contador - 1; i >= 0; i--) {
                solucaoCol[i] = fila[index].x;
                solucaoLin[i] = fila[index].y;
                index = fila[index].parent_index;
            }

            // Free memory
            for (int lin = 0; lin < num_linhas; lin++) free(visitado[lin]);
            free(visitado);
            free(fila);
            return 0;
        }

        // Explore neighbors
        for (int i = 0; i < 4; i++) {
            int nova_col = atual.x + delta_col[i];
            int nova_lin = atual.y + delta_lin[i];

            // Check bounds
            if (nova_col < 0 || nova_col >= num_colunas || nova_lin < 0 || nova_lin >= num_linhas) {
                continue;
            }

            // Check if it's a valid path and not visited
            if ((maze[nova_lin][nova_col] == CAMINHO_LIVRE || (nova_col == col_destino && nova_lin == lin_destino)) &&
                visitado[nova_lin][nova_col] == -1) {
                visitado[nova_lin][nova_col] = inicio - 1;
                fila[fim++] = (Node){.x = nova_col, .y = nova_lin, .parent_index = inicio - 1};
            }
        }
    }

    // Free memory on failure
    for (int lin = 0; lin < num_linhas; lin++) free(visitado[lin]);
    free(visitado);
    free(fila);
    return 1;
}

// Mark the path on the map
void marcarCaminho(char **maze, int solucaoX[], int solucaoY[], int tamanho_solucao) {
    if (tamanho_solucao <= 0 || maze == NULL || solucaoX == NULL || solucaoY == NULL) {
        return;
    }

    for (int i = 0; i < tamanho_solucao - 1; i++) {
        int x_atual = solucaoX[i];
        int y_atual = solucaoY[i];
        int x_prox = solucaoX[i + 1];
        int y_prox = solucaoY[i + 1];

        // Determine direction
        if (x_prox == x_atual + 1 && y_prox == y_atual) {
            maze[y_atual][x_atual] = DIREITA; // →
        } else if (x_prox == x_atual - 1 && y_prox == y_atual) {
            maze[y_atual][x_atual] = ESQUERDA; // ←
        } else if (y_prox == y_atual + 1 && x_prox == x_atual) {
            maze[y_atual][x_atual] = BAIXO; // ↓
        } else if (y_prox == y_atual - 1 && x_prox == x_atual) {
            maze[y_atual][x_atual] = CIMA; // ↑
        }
    }

    // Keep the destination marker
    int last_x = solucaoX[tamanho_solucao - 1];
    int last_y = solucaoY[tamanho_solucao - 1];
    maze[last_y][last_x] = DESTINO;
}

// Draw a square on the map
static void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda) {
    // Top: draw the top border
    for (int i = 0; i < largura_borda; i++) {
        for (int col = q.x; col < q.x + q.tamanho && col < mapa->colunas; col++) {
            if (q.y + i < mapa->linhas) {
                mapa->matriz[q.y + i][col] = RUA;
            }
        }
    }

    // Bottom: draw the bottom border
    for (int i = 0; i < largura_borda; i++) {
        for (int col = q.x; col < q.x + q.tamanho && col < mapa->colunas; col++) {
            if (q.y + q.tamanho - i - 1 < mapa->linhas) {
                mapa->matriz[q.y + q.tamanho - i - 1][col] = RUA;
            }
        }
    }

    // Sides: draw the left and right borders
    for (int i = 0; i < largura_borda; i++) {
        for (int lin = q.y; lin < q.y + q.tamanho && lin < mapa->linhas; lin++) {
            if (q.x + i < mapa->colunas) {
                mapa->matriz[lin][q.x + i] = RUA;
            }
            if (q.x + q.tamanho - i - 1 < mapa->colunas) {
                mapa->matriz[lin][q.x + q.tamanho - i - 1] = RUA;
            }
        }
    }
}

// Draw a road on the map
static void desenharRua(Mapa *mapa, int x1, int y1, int x2, int y2) {
    if (x1 != x2) {
        int step = (x2 > x1) ? 1 : -1;
        for (int x = x1; x != x2; x += step) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int y = y1 + k - mapa->largura_rua / 2;
                if (x >= 0 && x < mapa->colunas && y >= 0 && y < mapa->linhas) {
                    mapa->matriz[y][x] = RUA;
                }
            }
        }
    }

    if (y1 != y2) {
        int step = (y2 > y1) ? 1 : -1;
        for (int y = y1; y != y2; y += step) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int x = x2 + k - mapa->largura_rua / 2;
                if (x >= 0 && x < mapa->colunas && y >= 0 && y < mapa->linhas) {
                    mapa->matriz[y][x] = RUA;
                }
            }
        }
    }
}

// Connect squares using MST
static void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados) {
    if (num_quadrados <= 0 || quadrados == NULL) return;

    int *parent = malloc(num_quadrados * sizeof(int));
    int *key = malloc(num_quadrados * sizeof(int));
    bool *inMST = malloc(num_quadrados * sizeof(bool));

    // Initialize MST arrays
    for (int i = 0; i < num_quadrados; i++) {
        key[i] = INT_MAX;
        inMST[i] = false;
    }
    key[0] = 0;
    parent[0] = -1;

    // Prim's algorithm
    for (int count = 0; count < num_quadrados - 1; count++) {
        int minKey = INT_MAX, u = -1;

        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v] && key[v] < minKey) {
                minKey = key[v];
                u = v;
            }
        }

        inMST[u] = true;

        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v]) {
                int dx = abs((quadrados[u].x + quadrados[u].tamanho / 2) -
                             (quadrados[v].x + quadrados[v].tamanho / 2));
                int dy = abs((quadrados[u].y + quadrados[u].tamanho / 2) -
                             (quadrados[v].y + quadrados[v].tamanho / 2));
                int weight = dx + dy;

                if (weight < key[v]) {
                    key[v] = weight;
                    parent[v] = u;
                }
            }
        }
    }

    // Connect the squares
    for (int i = 1; i < num_quadrados; i++) {
        int u = parent[i];
        int x1, y1, x2, y2;
        encontrarPontosConexao(quadrados[u], quadrados[i], &x1, &y1, &x2, &y2);
        desenharRua(mapa, x1, y1, x2, y2);
    }

    free(parent);
    free(key);
    free(inMST);
}


static void encontrarPontosConexao(Quadrado a, Quadrado b, int *px1, int *py1, int *px2, int *py2)
{
    // Pontos médios nas bordas (otimizado)
    int a_centro_x = a.x + a.tamanho / 2; // coluna
    int a_centro_y = a.y + a.tamanho / 2; // linha
    int b_centro_x = b.x + b.tamanho / 2;
    int b_centro_y = b.y + b.tamanho / 2;

    // Determina a direção relativa entre quadrados
    int delta_x = b_centro_x - a_centro_x;
    int delta_y = b_centro_y - a_centro_y;

    // Escolhe pontos de conexão baseado na posição relativa
    if (abs(delta_x) > abs(delta_y))
    {
    // Conectar horizontalmente
    *px1 = (delta_x > 0) ? (a.x + a.tamanho - 1) : a.x; // Direita ou esquerda
    *py1 = a_centro_y;
    *px2 = (delta_x > 0) ? b.x : (b.x + b.tamanho - 1);
    *py2 = b_centro_y;
    }
    else
    {
    // Conectar verticalmente
    *px1 = a_centro_x;
    *py1 = (delta_y > 0) ? (a.y + a.tamanho - 1) : a.y; // Base ou topo
    *px2 = b_centro_x;
    *py2 = (delta_y > 0) ? b.y : (b.y + b.tamanho - 1);
}
}

// Finds a random free point on the map
bool find_random_free_point(Mapa* mapa, int* random_x, int* random_y) {
    const int max_attempts = 1000;
    int attempts = 0;

    if (!mapa || !mapa->matriz) {
        printf("Error: Map is not initialized.\n");
        return false;
    }

    do {
        *random_x = rand() % mapa->colunas;
        *random_y = rand() % mapa->linhas;
        attempts++;
    } while (mapa->matriz[*random_y][*random_x] != CAMINHO_LIVRE && attempts < max_attempts);

    if (attempts >= max_attempts) {
        printf("Error: Could not find a free position on the map.\n");
        return false;
    }

    return true;
}

// -------------------- THREAD FUNCTIONS --------------------

// User input thread
void* input_thread(void* arg) {
    ControlCenter* center = (ControlCenter*)arg;

    // Configure terminal for non-blocking input
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Save old terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char key;
    while (1) {
        // Non-blocking read
        key = getchar();

        if (key != EOF) {
            if (key == '\033') { // Escape sequence for arrow keys
                getchar();       // Skip the '[' character
                key = getchar(); // Get the actual arrow key
                switch (key) {
                    case 'A': // Up arrow
                        printf("Up arrow pressed: Creating a taxi.\n");
                        enqueue_message(&center->queue, CREATE_TAXI, 0, 0, 0, 0, NULL);
                        break;
                    case 'B': // Down arrow
                        printf("Down arrow pressed: Destroying a taxi.\n");
                        enqueue_message(&center->queue, DESTROY_TAXI, 0, 0, 0, 0, NULL);
                        break;
                }
            } else {
                // Process other keys
                switch (key) {
                    case 'r': // Reset the map
                        printf("Key 'r' pressed: Resetting the map.\n");
                        enqueue_message(&center->queue, RESET_MAP, 0, 0, 0, 0, NULL);
                        break;

                    case 'p': // Add a passenger
                        printf("Key 'p' pressed: Adding a passenger.\n");
                        enqueue_message(&center->queue, CREATE_PASSENGER, 0, 0, 0, 0, NULL);
                        break;

                    case 's': // Status request
                        printf("Key 's' pressed: Requesting status.\n");
                        enqueue_message(&center->queue, STATUS_REQUEST, 0, 0, 0, 0, NULL);
                        break;

                    case 'q': // Quit the program
                        printf("Key 'q' pressed: Exiting the program.\n");
                        enqueue_message(&center->queue, EXIT_PROGRAM, 0, 0, 0, 0, NULL);
                        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore old terminal settings
                        return NULL;

                    default:
                        printf("Unknown key '%c' pressed.\n", key);
                        break;
                }
            }
        }

        // Refresh rate (e.g., 100ms)
        usleep(100000);
    }

    // Restore old terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return NULL;
}

// Control center thread
void* control_center_thread(void* arg) {
    ControlCenter* center = (ControlCenter*)arg;

    // Access the visualizer queue (assumes it's passed via the center structure)
    MessageQueue* visualizerQueue = center->visualizerQueue;

    while (1) {
        // Dequeue a message
        Message* msg = dequeue_message(&center->queue);
        printf("Reading message: %d\n", msg->type);

        // Process the message
        switch (msg->type) {
            case CREATE_PASSENGER:
                pthread_mutex_lock(&center->lock);
                center->numPassengers++;
                printf("Passenger created. Total passengers: %d\n", center->numPassengers);
                pthread_mutex_unlock(&center->lock);

                // Forward the message to the visualizer thread
                enqueue_message(visualizerQueue, CREATE_PASSENGER, 0, 0, 0, 0, NULL);
                break;

            case STATUS_REQUEST:
                pthread_mutex_lock(&center->lock);
                printf("Control Center Status: ACTIVE\n");
                printf("Sending STATUS_REQUEST to all taxis.\n");
                printf("Number of taxis: %d\n", center->numTaxis);
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        priority_enqueue_message(&taxi->queue, STATUS_REQUEST, 0, 0, 0, 0, NULL);
                    }
                }
                pthread_mutex_unlock(&center->lock);
                break;

            case CREATE_TAXI: {
                pthread_mutex_lock(&center->lock);

                if (center->numTaxis >= MAX_TAXIS) {
                    printf("Cannot create more taxis. Maximum limit (%d) reached.\n", MAX_TAXIS);
                    pthread_mutex_unlock(&center->lock);
                    break;
                }

                // Allocate and initialize a new taxi
                Taxi* new_taxi = malloc(sizeof(Taxi));
                if (!new_taxi) {
                    perror("Failed to allocate memory for taxi");
                    pthread_mutex_unlock(&center->lock);
                    break;
                }

                new_taxi->id = center->numTaxis + 1;
                new_taxi->x = -1;
                new_taxi->y = -1;
                new_taxi->isFree = true;
                new_taxi->currentPassenger = -1;
                new_taxi->visualizerQueue = center->visualizerQueue;
                new_taxi->control_queue = &center->queue;
                pthread_mutex_init(&new_taxi->lock, NULL);
                init_queue(&new_taxi->queue);

                // Create the taxi thread
                new_taxi->thread_id = create_taxi_thread(new_taxi);
                // Store the taxi in the array
                center->taxis[center->numTaxis] = new_taxi;
                center->numTaxis++;

                printf("Taxi %d created and thread started.\n", new_taxi->id);

                pthread_mutex_unlock(&center->lock);
                break;
            }

            case DESTROY_TAXI: {
                pthread_mutex_lock(&center->lock);

                if (center->numTaxis <= 0) {
                    printf("No taxis to destroy.\n");
                    pthread_mutex_unlock(&center->lock);
                    break;
                }

                // Find a free taxi to destroy
                int taxi_index_to_destroy = -1;
                for (int i = center->numTaxis - 1; i >= 0; i--) {
                    if (center->taxis[i]->isFree) {
                        taxi_index_to_destroy = i;
                        break;
                    }
                }

                if (taxi_index_to_destroy == -1) {
                    printf("No free taxis available to destroy.\n");
                    pthread_mutex_unlock(&center->lock);
                    break;
                }

                // Get the taxi to destroy
                Taxi* taxi_to_destroy = center->taxis[taxi_index_to_destroy];

                // Send EXIT message to the taxi
                enqueue_message(&taxi_to_destroy->queue, EXIT, 0, 0, 0, 0, NULL);

                // Wait for the taxi thread to terminate
                pthread_join(taxi_to_destroy->thread_id, NULL);

                // Clean up the taxi
                pthread_mutex_destroy(&taxi_to_destroy->lock);
                cleanup_queue(&taxi_to_destroy->queue); // Free all messages in the queue
                free(taxi_to_destroy);

                // Remove the taxi from the array and shift remaining taxis
                for (int i = taxi_index_to_destroy; i < center->numTaxis - 1; i++) {
                    center->taxis[i] = center->taxis[i + 1];
                    center->taxis[i]->id = i + 1; // Update the ID to maintain consistency
                }
                center->taxis[center->numTaxis - 1] = NULL;
                center->numTaxis--;

                printf("Taxi destroyed. Remaining taxis: %d\n", center->numTaxis);

                pthread_mutex_unlock(&center->lock);
                break;
            }

            case RESET_MAP: {
                printf("Resetting the map and destroying all taxis.\n");

                pthread_mutex_lock(&center->lock);

                // Send EXIT to all taxis
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        enqueue_message(&taxi->queue, EXIT, 1, 0, 0, 0, NULL);
                    }
                }

                // Wait for all taxi threads to terminate
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        pthread_join(taxi->thread_id, NULL);

                        // Clean up the taxi
                        pthread_mutex_destroy(&taxi->lock);
                        cleanup_queue(&taxi->queue);
                        free(taxi);
                    }
                }

                // Reset the taxi array and counter
                center->numTaxis = 0;
                for (int i = 0; i < MAX_TAXIS; i++) {
                    center->taxis[i] = NULL;
                }

                pthread_mutex_unlock(&center->lock);

                // Forward the RESET_MAP command to the visualizer
                enqueue_message(visualizerQueue, RESET_MAP, 0, 0, 0, 0, NULL);
                break;
            }

            case RANDOM_REQUEST:
                pthread_mutex_lock(&center->lock);
                printf("Control Center: Received RANDOM_REQUEST from Taxi ID %d at position (%d, %d).\n",
                    msg->extra_x, msg->data_x, msg->data_y);
                pthread_mutex_unlock(&center->lock);

                // Forward the message to the visualizer
                enqueue_message(visualizerQueue, RANDOM_REQUEST, msg->data_x, msg->data_y, msg->extra_x, 0, NULL);

                break;

            case ROUTE_PLAN: {
                //printf("Control Center: Received ROUTE_PLAN for Taxi ID %d.\n", msg->extra_x);
                
                // Extract the PathData structure from the message
                PathData* path_data = (PathData*)msg->pointer;
                
                if (path_data) {
                    //printf("Control Center: Path length: %d\n", path_data->tamanho_solucao);
                
                    // Declare the taxi variable outside the loop
                    Taxi* taxi = NULL;
            
                        // Find the taxi by ID
                    pthread_mutex_lock(&center->lock);
                    for (int j = 0; j < center->numTaxis; j++) {
                        if (center->taxis[j]->id == msg->extra_x) {
                            taxi = center->taxis[j];
                            break;
                        }
                    }
                    pthread_mutex_unlock(&center->lock);
                
                    if (taxi) {
                        // Iterate through the path and send MOVE_TO messages to the taxi
                        for (int i = 0; i < path_data->tamanho_solucao; i++) {
                            int next_x = path_data->solucaoX[i];
                            int next_y = path_data->solucaoY[i];
            
                            //printf("Control Center: Sending MOVE_TO to Taxi ID %d: (%d, %d).\n", msg->extra_x, next_x, next_y);
                
                            // Send the MOVE_TO message to the taxi
                            enqueue_message(&taxi->queue, MOVE_TO, next_x, next_y, 0, 0, NULL);
                        }
            
                        // Send a FINISH message to the taxi after completing the route
                        //printf("Control Center: Sending FINISH to Taxi ID %d.\n", msg->extra_x);
                        enqueue_message(&taxi->queue, FINISH, 0, 0, 0, 0, NULL);
                    } else {
                        printf("Control Center: Taxi ID %d not found.\n", msg->extra_x);
                    }
                
                    // Free the PathData structure
                    free(path_data->solucaoX);
                    free(path_data->solucaoY);
                    free(path_data);
                } else {
                    printf("Control Center: No path data received.\n");
                }
            
                break;
            }

            case EXIT_PROGRAM:
                pthread_mutex_lock(&center->lock);

                // Send EXIT message to all taxis with priority
                printf("Sending EXIT message to all taxis.\n");
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        priority_enqueue_message(&taxi->queue, EXIT, 1, 0, 0, 0, NULL);
                    }
                }

                // Wait for all taxi threads to terminate
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        pthread_join(taxi->thread_id, NULL);

                        // Clean up the taxi
                        pthread_mutex_destroy(&taxi->lock);
                        cleanup_queue(&taxi->queue);
                        free(taxi);
                    }
                }

                center->numTaxis = 0; // Reset the taxi count

                // Send EXIT message to the visualizer thread
                enqueue_message(visualizerQueue, EXIT, 0, 0, 0, 0, NULL);
                pthread_mutex_unlock(&center->lock);

                printf("Exiting control center thread.\n");
                free(msg);
                return NULL;

            default:
                printf("Unknown message type received by CC. %d\n", msg->type);
                break;
        }

        free(msg);
    }

    return NULL;
}

// Visualizer thread
void* visualizador_thread(void* arg) {
    Visualizer* visualizer = (Visualizer*)arg;

    // Create the map
    Mapa* mapa = criarMapa();
    if (!mapa) return NULL;

    // Generate the map
    gerarMapa(mapa, visualizer->numQuadrados, visualizer->larguraRua, visualizer->larguraBorda,
              visualizer->minTamanho, visualizer->maxTamanho, visualizer->distanciaMinima);

    // Print the map after generation
    imprimirMapaLogico(mapa);
    renderMap(mapa); // TODO: DEIXAR APENAS O RENDER DEPOIS
    while (1) {
        // Dequeue a message
        Message* msg = dequeue_message(&visualizer->queue);

        switch (msg->type) {
            case RANDOM_REQUEST: {
                //printf("Visualizer: Received RANDOM_REQUEST from Taxi ID %d at position (%d, %d).\n", msg->extra_x, msg->data_x, msg->data_y);

                // Save the taxi's current position and ID
                int taxi_x = msg->data_x;
                int taxi_y = msg->data_y;
                int taxi_id = msg->extra_x;

                // Find a random free point on the map
                int random_x, random_y;
                if (!find_random_free_point(mapa, &random_x, &random_y)) {
                    printf("Visualizer: Could not find a random free point for Taxi ID %d.\n", taxi_id);
                    break;
                }
                //printf("Visualizer: Found random point at (%d, %d) for Taxi ID %d.\n", random_x, random_y, taxi_id);

                // Create vectors to store the solution path
                int* solucaoX = malloc(10000 * sizeof(int));
                int* solucaoY = malloc(10000 * sizeof(int));
                int tamanho_solucao = 0;

                // Find the path using encontrarCaminhoCoordenadas
                if (encontraCaminhoCoordenadas(taxi_x, taxi_y, random_x, random_y, mapa->matriz, mapa->colunas, mapa->linhas,
                                               solucaoX, solucaoY, &tamanho_solucao) == 0) {
                    //printf("Visualizer: Path found for Taxi ID %d. Path length: %d\n", taxi_id, tamanho_solucao);

                    // Allocate and populate the PathData structure
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = solucaoX;
                    path_data->solucaoY = solucaoY;
                    path_data->tamanho_solucao = tamanho_solucao;

                    // Send the ROUTE_PLAN message to the control center
                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, 0, path_data);
                    //printf("Visualizer: Sent ROUTE_PLAN to control center for Taxi ID %d.\n", taxi_id);
                } else {
                    printf("Visualizer: Pathfinding failed for Taxi ID %d.\n", taxi_id);
                    free(solucaoX);
                    free(solucaoY);
                }

                break;
            }

            case CREATE_PASSENGER: {
                printf("Adding a passenger.\n");

                // Ensure the map matrix is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }

                int random_x, random_y;

                // Find a random free position on the map
                if (!find_random_free_point(mapa, &random_x, &random_y)) {
                    printf("Error: Could not find a free position for the passenger.\n");
                    break;
                }

                // Add the passenger to the map
                mapa->matriz[random_y][random_x] = PASSENGER;
                imprimirMapaLogico(mapa); // Prints the map after adding a passenger
                renderMap(mapa);
                break;
            }

            case RESET_MAP: {
                printf("Resetting the map.\n");

                // Ensure the map is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }

                // Deallocate the old map
                desalocarMapa(mapa);

                // Create a new map
                mapa = criarMapa();
                if (!mapa) {
                    printf("Error: Failed to create a new map.\n");
                    break;
                }

                // Regenerate the map
                gerarMapa(mapa, visualizer->numQuadrados, visualizer->larguraRua, visualizer->larguraBorda,
                          visualizer->minTamanho, visualizer->maxTamanho, visualizer->distanciaMinima);

                // Print the new map
                imprimirMapaLogico(mapa);
                renderMap(mapa);
                break;
            }
            
            case SPAWN_TAXI: {
                printf("Visualizer: Received SPAWN_TAXI request.\n");

                // Garantir que o mapa está válido
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }

                // Calcular um ponto aleatório no mapa
                int random_x, random_y;

                if (!find_random_free_point(mapa, &random_x, &random_y)) {
                    printf("Error: Could not find a free position for the taxi.\n");
                    break;
                }

                // Obter o ponteiro para a fila do táxi a partir do campo pointer
                MessageQueue* taxi_queue = (MessageQueue*)msg->pointer;

                // Enviar SPAWN_TAXI para a fila do táxi com as coordenadas calculadas
                enqueue_message(taxi_queue, SPAWN_TAXI, random_x, random_y, 0, 0, NULL);
                printf("Visualizer: Sent SPAWN_TAXI to taxi queue with coordinates (%d, %d).\n", random_x, random_y);
                // Send FINISH to the taxi after it spawns
                enqueue_message(taxi_queue, FINISH, 0, 0, 0, 0, NULL);
                printf("Visualizer: Sent FINISH to taxi queue.\n");
                break;
            }

            case MOVE_TO: {
                printf("Visualizer: Moving taxi from (%d, %d) to (%d, %d).\n", 
                       msg->data_x, msg->data_y, msg->extra_x, msg->extra_y);
            
                // Ensure the map is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }
            
                // Handle the special case where the taxi is exiting
                if (msg->extra_x == -1 && msg->extra_y == -1) {
                    // Remove the taxi from the map
                    if (msg->data_x >= 0 && msg->data_y >= 0) {
                        mapa->matriz[msg->data_y][msg->data_x] = RUA; // Clear the old position
                    }
                    renderMap(mapa);
                    break;
                }
            
                // Update the map: move the taxi
                mapa->matriz[msg->extra_y][msg->extra_x] = TAXI; // Place the taxi in the new position
                if (msg->data_x >= 0 && msg->data_y >= 0) { // Check if the old position is valid
                    mapa->matriz[msg->data_y][msg->data_x] = RUA; // Clear the old position
                }
            
                // Render the updated map
                renderMap(mapa);
                break;
            }

            case EXIT:
                printf("Exiting visualizer thread.\n");
                desalocarMapa(mapa);
                free(msg);
                return NULL;

            default:
                printf("Unknown message type received by visualizer %d\n", msg->type);
                break;
        }

        free(msg);
    }
}

// Taxi thread
void* taxi_thread(void* arg) {
    Taxi* taxi = (Taxi*)arg;

    printf("Taxi %d thread started at position (%d, %d).\n", taxi->id, taxi->x, taxi->y);
    printf("Taxi %d visualizer queue: %p\n", taxi->id, &taxi->visualizerQueue);
    enqueue_message(taxi->visualizerQueue, SPAWN_TAXI, taxi->x, taxi->y, 0, 0, &taxi->queue);
    while (1) {
        // Dequeue a message
        Message* msg = dequeue_message(&taxi->queue);

        // Process the message
        switch (msg->type) {
            case SPAWN_TAXI: 
                printf("Taxi %d received SPAWN_TAXI: Moving to (%d, %d).\n", 
                       taxi->id, msg->data_x, msg->data_y);
            
                // Enviar a mensagem MOVE_TO para o visualizador
                enqueue_message(taxi->visualizerQueue, MOVE_TO, 
                                taxi->x, taxi->y, msg->data_x, msg->data_y, NULL);
            
                // Atualizar a posição do táxi para o destino
                pthread_mutex_lock(&taxi->lock);
                taxi->x = msg->data_x;
                taxi->y = msg->data_y;
                pthread_mutex_unlock(&taxi->lock);
            
                break;
            
            case MOVE_TO: 
                printf("Taxi %d received MOVE_TO: Move to (%d, %d).\n", taxi->id, msg->data_x, msg->data_y);
                
                usleep(1000000); 
                
                pthread_mutex_lock(&taxi->lock);
                int old_x = taxi->x;
                int old_y = taxi->y;
                taxi->x = msg->data_x;
                taxi->y = msg->data_y;
                pthread_mutex_unlock(&taxi->lock);
                
                enqueue_message(taxi->visualizerQueue, MOVE_TO, old_x, old_y, msg->data_x, msg->data_y, NULL);
                
                break;
                              
            case FINISH:
                printf("Taxi %d received FINISH command. Reporting to control center.\n", taxi->id);

                // Send RANDOM_REQUEST to the control center
                enqueue_message(taxi->control_queue, RANDOM_REQUEST, taxi->x, taxi->y, taxi->id, 0, NULL);

                break;   

            case EXIT:
                printf("Taxi %d received EXIT command. Terminating thread.\n", taxi->id);
                if (msg->data_x == 1) {
                    printf("Taxi %d exiting due to program termination or reset.\n", taxi->id);
                } else {
                    printf("Taxi %d was killed.\n", taxi->id);
                    enqueue_message(taxi->visualizerQueue, MOVE_TO, taxi->x, taxi->y, -1, -1, NULL);
                }
                free(msg);
                return NULL;

            case STATUS_REQUEST:
                printf("Taxi %d status request: Position (%d, %d), Free: %s\n", taxi->id, taxi->x, taxi->y,
                       taxi->isFree ? "Yes" : "No");
                break;

            default:
                printf("Taxi %d received unknown message type: %d\n", taxi->id, msg->type);
                break;
        }
        free(msg);
    }
}

// Create a taxi thread
pthread_t create_taxi_thread(Taxi* taxi) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, taxi_thread, taxi) != 0) {
        perror("Failed to create taxi thread");
        exit(EXIT_FAILURE);
    }
    return thread;
}

// -------------------- MAIN FUNCTION --------------------

void init_operations() {
    // Initialize the control center
    ControlCenter center;
    center.numPassengers = 0;
    center.numTaxis = 0;
    pthread_mutex_init(&center.lock, NULL);
    init_queue(&center.queue);

    for (int i = 0; i < MAX_TAXIS; i++) {
        center.taxis[i] = NULL; // Initialize taxi array
    }

    // Initialize the visualizer
    Visualizer visualizer = {10, 2, 2, 5, 13, 2};
    init_queue(&visualizer.queue);

    // Link the visualizer queue to the control center
    center.visualizerQueue = &visualizer.queue;

    // Link the control queue to the visualizer
    visualizer.control_queue = &center.queue;

    // Create threads
    pthread_t inputThread, controlCenterThread, visualizerThread;
    pthread_create(&inputThread, NULL, input_thread, &center);
    pthread_create(&controlCenterThread, NULL, control_center_thread, &center);
    pthread_create(&visualizerThread, NULL, visualizador_thread, &visualizer);

    // Wait for threads to finish
    pthread_join(inputThread, NULL);
    pthread_join(controlCenterThread, NULL);
    pthread_join(visualizerThread, NULL);

    // Clean up
    pthread_mutex_destroy(&center.lock);
    pthread_mutex_destroy(&center.queue.lock);
    pthread_cond_destroy(&center.queue.cond);
    pthread_mutex_destroy(&visualizer.queue.lock);
    pthread_cond_destroy(&visualizer.queue.cond);
    printf("Program terminated.\n");
}

int main() {
    init_operations();
    return 0;
}