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
    EXIT
} MessageType;

// Message structure
typedef struct Message {
    MessageType type;
    struct Message* next;
    int data_x;
    int data_y;
    int extra_x;
    int extra_y;
} Message;

// Message queue structure
typedef struct {
    Message* head;
    Message* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} MessageQueue;

// Control center structure
typedef struct {
    int numPassengers;
    pthread_mutex_t lock;
    MessageQueue queue;
    MessageQueue* visualizerQueue; // Added visualizer queue reference
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
} InitVisualizer;


// Function prototypes
static void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda);
static void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados);
static void encontrarPontosConexao(Quadrado a, Quadrado b, int *px1, int *py1, int *px2, int *py2);
void init_operations();
void renderMap(Mapa* mapa);

// -------------------- QUEUE FUNCTIONS --------------------

// Initialize the queue
void init_queue(MessageQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// Enqueue a message
void enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y) {
    Message* new_msg = malloc(sizeof(Message));
    new_msg->type = type;
    new_msg->data_x = x;
    new_msg->data_y = y;
    new_msg->extra_x = extra_x;
    new_msg->extra_y = extra_y;
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
                default:
                    // For now, treat all other symbols (e.g., passengers) as a person emoji
                    printf(PASSENGER_EMOJI);
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
            // Process the key
            switch (key) {
                case 'r': // Reset the map
                    printf("Key 'r' pressed: Resetting the map.\n");
                    enqueue_message(&center->queue, RESET_MAP, 0, 0, 0, 0);
                    break;

                case 'p': // Add a passenger
                    printf("Key 'p' pressed: Adding a passenger.\n");
                    enqueue_message(&center->queue, CREATE_PASSENGER, 0, 0, 0, 0);
                    break;

                case 'q': // Quit the program
                    printf("Key 'q' pressed: Exiting the program.\n");
                    enqueue_message(&center->queue, EXIT_PROGRAM, 0, 0, 0, 0);
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore old terminal settings
                    return NULL;

                default:
                    printf("Unknown key '%c' pressed.\n", key);
                    break;
            }
        }

        // Refresh rate (e.g., 100ms)
        usleep(1000);
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
                enqueue_message(visualizerQueue, CREATE_PASSENGER,0, 0, 0, 0);
                break;

            case RESET_MAP:
                printf("Resetting the map.\n");

                pthread_mutex_lock(&center->lock);
                enqueue_message(visualizerQueue, RESET_MAP, 0, 0, 0, 0); // Signal the visualizer thread to exit
                pthread_mutex_unlock(&center->lock);

                break;

            case EXIT_PROGRAM:
                // Terminate all threads and restart
                pthread_mutex_lock(&center->lock);
                enqueue_message(visualizerQueue, EXIT, 0, 0, 0, 0); // Signal the visualizer thread to exit
                pthread_mutex_unlock(&center->lock);

                // Wait for the visualizer thread to exit
                pthread_join(pthread_self(), NULL);
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
    InitVisualizer* visualizer = (InitVisualizer*)arg;

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
            case PATHFIND_REQUEST: {
                int solucaoX[10000], solucaoY[10000], tamanho_solucao = 0;
                if (encontraCaminho(msg->data_x, msg->data_y, mapa->matriz, mapa->colunas, mapa->linhas,
                                    solucaoX, solucaoY, &tamanho_solucao) == 0) {
                    //marcarCaminho(mapa->matriz, solucaoX, solucaoY, tamanho_solucao);
                    imprimirMapaLogico(mapa); // Prints the map after marking the path
                } else {
                    printf("Pathfinding failed.\n");
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
                int attempts = 0;
                const int max_attempts = 1000;

                // Find a random free position on the map
                do {
                    random_x = rand() % mapa->colunas;
                    random_y = rand() % mapa->linhas;
                    attempts++;
                } while (mapa->matriz[random_y][random_x] != CAMINHO_LIVRE && attempts < max_attempts);

                if (attempts >= max_attempts) {
                    printf("Error: Could not find a free position for the passenger.\n");
                    break;
                }

                // Add the passenger to the map
                mapa->matriz[random_y][random_x] = 'A' + (rand() % 26); // Random letter for passenger
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

// -------------------- MAIN FUNCTION --------------------

void init_operations() {
    // Initialize the control center
    ControlCenter center;
    center.numPassengers = 0;
    pthread_mutex_init(&center.lock, NULL);
    init_queue(&center.queue);

    // Initialize the visualizer
    // int numQuadrados;
    // int larguraRua;
    // int larguraBorda;
    // int minTamanho;
    // int maxTamanho;
    // int distanciaMinima;
    InitVisualizer visualizer = {10, 2, 2, 5, 13, 2};
    init_queue(&visualizer.queue);

    // Link the visualizer queue to the control center
    center.visualizerQueue = &visualizer.queue;

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