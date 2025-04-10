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

// Commands
// p - Create passenger
// r - Reset map
// l - Print logical map
// s - Taxi status
// q - Quit
// ↑ - Create taxi
// ↓ - Destroy taxi
// Space - Pause/Resume (Note: visualizer continues running)

// Entities in the matrix
// 0 - Free path
// 1 - Sidewalk/Buildings
// 100 - 199 Free taxis
// 200 - 299 Occupied taxis
// 300 - 399 Passengers
// 400 - 499 Passenger destinations
// 500 - 599 Taxi routes (Marking to avoid spawning passengers on taxi routes)

// -------------------- CONSTANTS --------------------

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_ATTEMPTS 1000
#define TAXI_REFRESH_RATE 100000
#define TAXI_SPEED_FACTOR 5
#define MAX_PASSENGERS 20
#define REFRESH_PASSENGERS_SEC 8

#define R_TAXI_FREE 100 // 100 - 199 free taxis
#define R_TAXI_OCCUPIED 200 // 200 - 299 occupied taxis
#define R_PASSENGER 300 // 300 - 399 passengers
#define R_PASSENGER_DEST 400 // 400 - 499 passenger destinations
#define R_PASSENGER_POINT 500 // 500 - 599 passenger points

#define ROAD 0
#define SIDEWALK 1

#define ORIGIN 'E'
#define DESTINATION 'D'
#define VISITED '-'
#define RIGHT '>'
#define LEFT '<'
#define UP '^'
#define DOWN 'v'

#define TAXI 'T'
#define PASSENGER 'P'

#define SIDEWALK_EMOJI "⬛"  
#define ROAD_EMOJI "⬜"       
#define PASSENGER_EMOJI "🙋" 
#define DESTINATION_EMOJI "🏠"   
#define TAXI_EMOJI "🚖"
#define PASSENGER_POINT_EMOJI "🔲"

#define MAP_VERTICAL_PROPORTION 0.8
#define MAP_HORIZONTAL_PROPORTION 0.5

#define MAX_TAXIS 6

// Global variables for pause/resume functionality
pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pause_cond = PTHREAD_COND_INITIALIZER;
bool isPaused = false;

// -------------------- STRUCTURES --------------------

// Node for BFS
typedef struct {
    int x;
    int y;
    int parent_index;
} Node;

// Square structure
typedef struct {
    int x, y;
    int size;
} Square;

// Map structure
typedef struct {
    int rows, cols;
    int road_width;
    int **matrix;
    pthread_mutex_t lock; 
} Map;

typedef struct {
    int id;
    int x_sidewalk;
    int y_sidewalk;
    int x_road;
    int y_road;
    bool isFree;
    int x_sidewalk_dest;
    int y_sidewalk_dest;
    int x_road_dest;
    int y_road_dest;
} Passenger;

// Message types
typedef enum {
    CREATE_PASSENGER,
    DELETE_PASSENGER,
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
    FINISH,
    PRINT_LOGICO,
    DROP,
    GOT_PASSENGER,
    ARRIVED_AT_DESTINATION,
    REFRESH_PASSENGERS
    
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
    pthread_cond_t drop_cond; 
    bool drop_processed; 
} Taxi;

// Control center structure
typedef struct {
    int numPassengers;
    pthread_mutex_t lock;
    MessageQueue queue;
    MessageQueue* visualizerQueue; 
    Taxi* taxis[MAX_TAXIS];
    int numTaxis;
    Passenger* passengers[MAX_PASSENGERS]; 
} ControlCenter;

// Visualizer structure
typedef struct {
    int numSquares;
    int roadWidth;
    int borderWidth;
    int minSize;
    int maxSize;
    int minDistance;
    MessageQueue queue;
    MessageQueue* control_queue;
} Visualizer;

// Function prototypes
static void drawSquare(Map* map, Square q, int borderWidth);
static void connectSquaresMST(Map* map, Square* squares, int num_squares);
static void findConnectionPoints(Square a, Square b, int* px1, int* py1, int* px2, int* py2);
void init_operations();
void renderMap(Map* map);
pthread_t create_taxi_thread(Taxi* taxi);
bool find_random_free_point(Map* map, int* random_x, int* random_y);

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
Map* createMap() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return NULL;
    }

    // Scale the terminal size by 0.6
    int scaled_rows = (int)(ws.ws_row * MAP_VERTICAL_PROPORTION);
    int scaled_cols = (int)(ws.ws_col * MAP_HORIZONTAL_PROPORTION);

    Map* map = malloc(sizeof(Map));
    map->rows = scaled_rows;
    map->cols = scaled_cols;
    map->road_width = 1;

    map->matrix = malloc(map->rows * sizeof(int*));
    for (int i = 0; i < map->rows; i++) {
        map->matrix[i] = malloc(map->cols * sizeof(int));
        for (int j = 0; j < map->cols; j++) {
            map->matrix[i][j] = SIDEWALK;
        }
    }

    return map;
}

// Free the map
void freeMap(Map* map) {
    if (!map) return;

    for (int i = 0; i < map->rows; i++) {
        free(map->matrix[i]);
    }
    free(map->matrix);
    free(map);
}

// Generate the map
void generateMap(Map* map, int num_squares, int road_width, int border_width, int min_size, int max_size, int min_distance) {
    map->road_width = road_width;
    srand(time(NULL));

    if (min_size <= 0 || max_size < min_size || map->rows <= 0 || map->cols <= 0) {
        return;
    }

    int max_possible_size = MIN(map->cols, map->rows) - 2 * border_width;
    if (max_size > max_possible_size) {
        max_size = max_possible_size;
        min_size = MIN(min_size, max_size);
    }

    Square squares[num_squares];
    int count = 0, attempts = 0;

    // Clear the map (initialize with sidewalks)
    for (int i = 0; i < map->rows; i++) {
        for (int j = 0; j < map->cols; j++) {
            map->matrix[i][j] = SIDEWALK;
        }
    }

    // Generate squares (blocks)
    while (count < num_squares && attempts < MAX_ATTEMPTS * num_squares) {
        int size = rand() % (max_size - min_size + 1) + min_size;

        int max_col = map->cols - size - border_width;
        int max_row = map->rows - size - border_width;

        int current_distance = min_distance;
        if (max_col <= 0 || max_row <= 0) {
            current_distance = 0;
            max_col = map->cols - size;
            max_row = map->rows - size;
            if (max_col <= 0 || max_row <= 0) break;
        }

        Square q = {
            .x = rand() % (max_col + 1),
            .y = rand() % (max_row + 1),
            .size = size
        };

        bool valid = true;
        for (int i = 0; i < count; i++) {
            int dx = abs((q.x + q.size / 2) - (squares[i].x + squares[i].size / 2));
            int dy = abs((q.y + q.size / 2) - (squares[i].y + squares[i].size / 2));
            if (dx < current_distance && dy < current_distance) {
                valid = false;
                break;
            }
        }

        if (valid) {
            squares[count++] = q;
            drawSquare(map, q, border_width);
            attempts = 0;
        } else {
            attempts++;
        }
    }

    // Connect squares using MST logic
    if (count > 0) {
        connectSquaresMST(map, squares, count);
    }
}

// Print the map
void printLogicalMap(Map* map) {
    for (int i = 0; i < map->rows; i++) {
        for (int j = 0; j < map->cols; j++) {
            printf("%d", map->matrix[i][j]);
        }
        printf("\n");
    }
}

// Render the map with emojis
void renderMap(Map* map) {
    if (!map || !map->matrix) {
        return;
    }
    printf("\033[H\033[J");
    for (int i = 0; i < map->rows; i++) {
        for (int j = 0; j < map->cols; j++) {
            switch (map->matrix[i][j]) {
                case SIDEWALK:
                    printf(SIDEWALK_EMOJI);
                    break;
                case ROAD:
                    printf(ROAD_EMOJI);
                    break;
                case DESTINATION:
                    printf(DESTINATION_EMOJI);
                    break;
                case PASSENGER:
                    printf(PASSENGER_EMOJI);
                    break;
                case TAXI:
                    printf(TAXI_EMOJI);
                    break;
                default:
                    if (map->matrix[i][j] >= R_PASSENGER && map->matrix[i][j] < R_PASSENGER + 100) {
                        printf(PASSENGER_EMOJI);
                        break;
                    }
                    if (map->matrix[i][j] >= R_TAXI_FREE && map->matrix[i][j] < R_TAXI_OCCUPIED + 100) {
                        printf(TAXI_EMOJI);
                        break;
                    }
                    if (map->matrix[i][j] >= R_PASSENGER_POINT && map->matrix[i][j] < R_PASSENGER_POINT + 100) {
                        printf(PASSENGER_POINT_EMOJI);
                        break;
                    }
                    if (map->matrix[i][j] >= R_PASSENGER_DEST && map->matrix[i][j] < R_PASSENGER_DEST + 100) {
                        printf(DESTINATION_EMOJI); // Empty space
                        break;
                    }
                    printf("?"); // Empty space
                    break;
            }
        }
        printf("\n");
    }
}

// Find path in the map
int findPath(int start_col, int start_row, int **maze, int num_cols, int num_rows,
                    int solutionCol[], int solutionRow[], int *solution_size, int destination) {
    // BFS queue
    Node *queue = malloc(num_cols * num_rows * sizeof(Node));
    int start = 0, end = 0;

    // Visited matrix
    int **visited = malloc(num_rows * sizeof(int *));
    for (int row = 0; row < num_rows; row++) {
        visited[row] = malloc(num_cols * sizeof(int));
        for (int col = 0; col < num_cols; col++) {
            visited[row][col] = -1;
        }
    }

    // Add the starting node
    queue[end++] = (Node){.x = start_col, .y = start_row, .parent_index = -1};
    visited[start_row][start_col] = 0;

    // Directions for moving (left, right, up, down)
    int delta_col[] = {0, 0, -1, 1};
    int delta_row[] = {-1, 1, 0, 0};

    // BFS loop
    while (start < end) {
        Node current = queue[start++];

        // Check if it's the destination
        if (maze[current.y][current.x] >= destination && maze[current.y][current.x] < destination + 100) {
            // Count the path length
            int counter = 0;
            int index = start - 1;
            while (index != -1) {
                counter++;
                index = queue[index].parent_index;
            }

            // Fill the path in the correct order (start -> destination)
            *solution_size = counter;
            index = start - 1;
            for (int i = counter - 1; i >= 0; i--) {
                solutionCol[i] = queue[index].x;
                solutionRow[i] = queue[index].y;
                index = queue[index].parent_index;
            }

            // Free memory
            for (int row = 0; row < num_rows; row++) free(visited[row]);
            free(visited);
            free(queue);
            return 0;
        }

        // Explore neighbors
        for (int i = 0; i < 4; i++) {
            int new_col = current.x + delta_col[i];
            int new_row = current.y + delta_row[i];

            // Check bounds
            if (new_col < 0 || new_col >= num_cols || new_row < 0 || new_row >= num_rows) {
                continue;
            }

            // Check if it's a valid path and not visited
            if ((maze[new_row][new_col] == ROAD ||( maze[new_row][new_col] >= destination && maze[new_row][new_col] < destination + 100)) &&
                visited[new_row][new_col] == -1) {
                visited[new_row][new_col] = start - 1;
                queue[end++] = (Node){.x = new_col, .y = new_row, .parent_index = start - 1};
            }
        }
    }

    // Free memory on failure
    for (int row = 0; row < num_rows; row++) free(visited[row]);
    free(visited);
    free(queue);
    return 1;
}

// Find path with specific coordinates
int findPathCoordinates(int start_col, int start_row, int dest_col, int dest_row,
                               int **maze, int num_cols, int num_rows,
                               int solutionCol[], int solutionRow[], int *solution_size) {
    // BFS queue
    Node *queue = malloc(num_cols * num_rows * sizeof(Node));
    int start = 0, end = 0;

    // Visited matrix
    int **visited = malloc(num_rows * sizeof(int *));
    for (int row = 0; row < num_rows; row++) {
        visited[row] = malloc(num_cols * sizeof(int));
        for (int col = 0; col < num_cols; col++) {
            visited[row][col] = -1;
        }
    }

    // Add the starting node
    queue[end++] = (Node){.x = start_col, .y = start_row, .parent_index = -1};
    visited[start_row][start_col] = 0;

    // Directions for moving (left, right, up, down)
    int delta_col[] = {0, 0, -1, 1};
    int delta_row[] = {-1, 1, 0, 0};

    // BFS loop
    while (start < end) {
        Node current = queue[start++];

        // Check if it's the destination
        if (current.x == dest_col && current.y == dest_row) {
            // Count the path length
            int counter = 0;
            int index = start - 1;
            while (index != -1) {
                counter++;
                index = queue[index].parent_index;
            }

            // Fill the path in the correct order (start -> destination)
            *solution_size = counter;
            index = start - 1;
            for (int i = counter - 1; i >= 0; i--) {
                solutionCol[i] = queue[index].x;
                solutionRow[i] = queue[index].y;
                index = queue[index].parent_index;
            }

            // Free memory
            for (int row = 0; row < num_rows; row++) free(visited[row]);
            free(visited);
            free(queue);
            return 0;
        }

        // Explore neighbors
        for (int i = 0; i < 4; i++) {
            int new_col = current.x + delta_col[i];
            int new_row = current.y + delta_row[i];

            // Check bounds
            if (new_col < 0 || new_col >= num_cols || new_row < 0 || new_row >= num_rows) {
                continue;
            }

            // Check if it's a valid path and not visited
            if ((maze[new_row][new_col] == ROAD || (new_col == dest_col && new_row == dest_row)) &&
                visited[new_row][new_col] == -1) {
                visited[new_row][new_col] = start - 1;
                queue[end++] = (Node){.x = new_col, .y = new_row, .parent_index = start - 1};
            }
        }
    }

    // Free memory on failure
    for (int row = 0; row < num_rows; row++) free(visited[row]);
    free(visited);
    free(queue);
    return 1;
}

// Mark the path on the map
void markPath(int **maze, int solutionX[], int solutionY[], int solution_size) {
    if (solution_size <= 0 || maze == NULL || solutionX == NULL || solutionY == NULL) {
        return;
    }

    for (int i = 0; i < solution_size - 1; i++) {
        int current_x = solutionX[i];
        int current_y = solutionY[i];
        int next_x = solutionX[i + 1];
        int next_y = solutionY[i + 1];

        // Determine direction
        if (next_x == current_x + 1 && next_y == current_y) {
            maze[current_y][current_x] = RIGHT; // →
        } else if (next_x == current_x - 1 && next_y == current_y) {
            maze[current_y][current_x] = LEFT; // ←
        } else if (next_y == current_y + 1 && next_x == current_x) {
            maze[current_y][current_x] = DOWN; // ↓
        } else if (next_y == current_y - 1 && next_x == current_x) {
            maze[current_y][current_x] = UP; // ↑
        }
    }

    // Keep the destination marker
    int last_x = solutionX[solution_size - 1];
    int last_y = solutionY[solution_size - 1];
    maze[last_y][last_x] = DESTINATION;
}

// Draw a square on the map
static void drawSquare(Map *map, Square q, int borderWidth) {
    // Top: draw the top border
    for (int i = 0; i < borderWidth; i++) {
        for (int col = q.x; col < q.x + q.size && col < map->cols; col++) {
            if (q.y + i < map->rows) {
                map->matrix[q.y + i][col] = ROAD;
            }
        }
    }

    // Bottom: draw the bottom border
    for (int i = 0; i < borderWidth; i++) {
        for (int col = q.x; col < q.x + q.size && col < map->cols; col++) {
            if (q.y + q.size - i - 1 < map->rows) {
                map->matrix[q.y + q.size - i - 1][col] = ROAD;
            }
        }
    }

    // Sides: draw the left and right borders
    for (int i = 0; i < borderWidth; i++) {
        for (int row = q.y; row < q.y + q.size && row < map->rows; row++) {
            if (q.x + i < map->cols) {
                map->matrix[row][q.x + i] = ROAD;
            }
            if (q.x + q.size - i - 1 < map->cols) {
                map->matrix[row][q.x + q.size - i - 1] = ROAD;
            }
        }
    }
}

// Draw a road on the map
static void drawRoad(Map *map, int x1, int y1, int x2, int y2) {
    if (x1 != x2) {
        int step = (x2 > x1) ? 1 : -1;
        for (int x = x1; x != x2; x += step) {
            for (int k = 0; k < map->road_width; k++) {
                int y = y1 + k - map->road_width / 2;
                if (x >= 0 && x < map->cols && y >= 0 && y < map->rows) {
                    map->matrix[y][x] = ROAD;
                }
            }
        }
    }

    if (y1 != y2) {
        int step = (y2 > y1) ? 1 : -1;
        for (int y = y1; y != y2; y += step) {
            for (int k = 0; k < map->road_width; k++) {
                int x = x2 + k - map->road_width / 2;
                if (x >= 0 && x < map->cols && y >= 0 && y < map->rows) {
                    map->matrix[y][x] = ROAD;
                }
            }
        }
    }
}

// Connect squares using MST
static void connectSquaresMST(Map *map, Square *squares, int num_squares) {
    if (num_squares <= 0 || squares == NULL) return;

    int *parent = malloc(num_squares * sizeof(int));
    int *key = malloc(num_squares * sizeof(int));
    bool *inMST = malloc(num_squares * sizeof(bool));

    // Initialize MST arrays
    for (int i = 0; i < num_squares; i++) {
        key[i] = INT_MAX;
        inMST[i] = false;
    }
    key[0] = 0;
    parent[0] = -1;

    // Prim's algorithm
    for (int count = 0; count < num_squares - 1; count++) {
        int minKey = INT_MAX, u = -1;

        for (int v = 0; v < num_squares; v++) {
            if (!inMST[v] && key[v] < minKey) {
                minKey = key[v];
                u = v;
            }
        }

        inMST[u] = true;

        for (int v = 0; v < num_squares; v++) {
            if (!inMST[v]) {
                int dx = abs((squares[u].x + squares[u].size / 2) -
                             (squares[v].x + squares[v].size / 2));
                int dy = abs((squares[u].y + squares[u].size / 2) -
                             (squares[v].y + squares[v].size / 2));
                int weight = dx + dy;

                if (weight < key[v]) {
                    key[v] = weight;
                    parent[v] = u;
                }
            }
        }
    }

    // Connect the squares
    for (int i = 1; i < num_squares; i++) {
        int u = parent[i];
        int x1, y1, x2, y2;
        findConnectionPoints(squares[u], squares[i], &x1, &y1, &x2, &y2);
        drawRoad(map, x1, y1, x2, y2);
    }

    free(parent);
    free(key);
    free(inMST);
}


static void findConnectionPoints(Square a, Square b, int *px1, int *py1, int *px2, int *py2)
{
    // Pontos médios nas bordas (otimizado)
    int a_center_x = a.x + a.size / 2; // coluna
    int a_center_y = a.y + a.size / 2; // linha
    int b_center_x = b.x + b.size / 2;
    int b_center_y = b.y + b.size / 2;

    // Determina a direção relativa entre quadrados
    int delta_x = b_center_x - a_center_x;
    int delta_y = b_center_y - a_center_y;

    // Escolhe pontos de conexão baseado na posição relativa
    if (abs(delta_x) > abs(delta_y))
    {
    // Conectar horizontalmente
    *px1 = (delta_x > 0) ? (a.x + a.size - 1) : a.x; // Direita ou esquerda
    *py1 = a_center_y;
    *px2 = (delta_x > 0) ? b.x : (b.x + b.size - 1);
    *py2 = b_center_y;
    }
    else
    {
    // Conectar verticalmente
    *px1 = a_center_x;
    *py1 = (delta_y > 0) ? (a.y + a.size - 1) : a.y; // Base ou topo
    *px2 = b_center_x;
    *py2 = (delta_y > 0) ? b.y : (b.y + b.size - 1);
}
}

// Finds a random free point on the map
bool find_random_free_point(Map* map, int* random_x, int* random_y) {
    const int max_attempts = 1000;
    int attempts = 0;

    if (!map || !map->matrix) {
        return false;
    }

    do {
        *random_x = rand() % map->cols;
        *random_y = rand() % map->rows;
        attempts++;
    } while (map->matrix[*random_y][*random_x] != ROAD && attempts < max_attempts);

    if (attempts >= max_attempts) {
        return false;
    }

    return true;
}

// Finds a random free point on the map that is adjacent to a SIDEWALK
bool find_random_free_point_adjacent_to_sidewalk(Map* map, int* free_x, int* free_y, int* sidewalk_x, int* sidewalk_y) {
    const int max_attempts = MAX_ATTEMPTS;
    int attempts = 0;

    if (!map || !map->matrix) {
        return false;
    }

    do {
        // Generate a random free point
        *free_x = rand() % map->cols;
        *free_y = rand() % map->rows;

        // Check if the point is free (ROAD)
        if (map->matrix[*free_y][*free_x] == ROAD) {
            // Check all adjacent points for a SIDEWALK
            int delta_x[] = {0, 0, -1, 1};
            int delta_y[] = {-1, 1, 0, 0};

            for (int i = 0; i < 4; i++) {
                int adj_x = *free_x + delta_x[i];
                int adj_y = *free_y + delta_y[i];

                // Ensure the adjacent point is within bounds
                if (adj_x >= 0 && adj_x < map->cols && adj_y >= 0 && adj_y < map->rows) {
                    // Check if the adjacent point is a SIDEWALK
                    if (map->matrix[adj_y][adj_x] == SIDEWALK) {
                        *sidewalk_x = adj_x;
                        *sidewalk_y = adj_y;
                        return true;
                    }
                }
            }
        }

        attempts++;
    } while (attempts < max_attempts);

    return false;
}

// -------------------- THREAD FUNCTIONS --------------------

void refresh_passengers(ControlCenter* center) {
    pthread_mutex_lock(&center->lock);

    for (int i = 0; i < center->numPassengers; i++) {
        Passenger* passenger = center->passengers[i];
        if (passenger) {
            // Check if the passenger is not currently assigned to a taxi
            bool assigned = false;
            for (int j = 0; j < center->numTaxis; j++) {
                Taxi* taxi = center->taxis[j];
                if (taxi && taxi->currentPassenger == passenger->id) {
                    assigned = true;
                    break;
                }
            }

            if (!assigned) {
                // Re-send CREATE_PASSENGER with existing coordinates
                enqueue_message(center->visualizerQueue, CREATE_PASSENGER,
                                passenger->x_road, passenger->y_road,
                                passenger->x_sidewalk, passenger->y_sidewalk, passenger);
            }
        }
    }

    pthread_mutex_unlock(&center->lock);
}

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
                        enqueue_message(&center->queue, CREATE_TAXI, 0, 0, 0, 0, NULL);
                        break;
                    case 'B': // Down arrow
                        enqueue_message(&center->queue, DESTROY_TAXI, 0, 0, 0, 0, NULL);
                        break;
                }
            } else {
                // Process other keys
                switch (key) {
                    case ' ': // Spacebar to toggle pause/play
                        pthread_mutex_lock(&pause_mutex);
                        isPaused = !isPaused;
                        if (!isPaused) {
                            pthread_cond_broadcast(&pause_cond); // Resume all threads
                        }
                        pthread_mutex_unlock(&pause_mutex);
                        break;

                    case 'r': // Reset the map
                        enqueue_message(&center->queue, RESET_MAP, 0, 0, 0, 0, NULL);
                        break;

                    case 'p': // Add a passenger
                        enqueue_message(&center->queue, CREATE_PASSENGER, 0, 0, 0, 0, NULL);
                        break;

                    case 's': // Status request
                        enqueue_message(&center->queue, STATUS_REQUEST, 0, 0, 0, 0, NULL);
                        break;

                    case 'l': // Print logical map
                        enqueue_message(center->visualizerQueue, PRINT_LOGICO, 0, 0, 0, 0, NULL);
                        break;

                    case 'q': // Quit the program
                        pthread_mutex_lock(&pause_mutex);
                        if (isPaused) {
                            isPaused = false; // Unpause the game
                            pthread_cond_broadcast(&pause_cond); // Resume all threads
                        }
                        pthread_mutex_unlock(&pause_mutex);

                        enqueue_message(&center->queue, EXIT_PROGRAM, 0, 0, 0, 0, NULL);
                        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore old terminal settings
                        return NULL;
                    default:
                        break;
                }
            }
        }

        usleep(500000);
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
        pthread_mutex_lock(&pause_mutex);
        while (isPaused) {
            pthread_cond_wait(&pause_cond, &pause_mutex);
        }
        pthread_mutex_unlock(&pause_mutex);
        // Dequeue a message
        Message* msg = dequeue_message(&center->queue);

        // Process the message
        switch (msg->type) {
            case CREATE_PASSENGER:
                pthread_mutex_lock(&center->lock);

                if (center->numPassengers >= MAX_PASSENGERS) {
                    pthread_mutex_unlock(&center->lock);
                    break;
                }
            
                // Allocate memory for a new passenger
                Passenger* new_passenger = malloc(sizeof(Passenger));
                if (!new_passenger) {
                    pthread_mutex_unlock(&center->lock);
                    break;
                }
            
                // Assign an ID and initialize the passenger
                new_passenger->id = center->numPassengers + 1;
                new_passenger->x_sidewalk = -1; // Placeholder values
                new_passenger->y_sidewalk = -1;
                new_passenger->x_road = -1;
                new_passenger->y_road = -1;
            
                // Store the passenger in the vector
                center->passengers[center->numPassengers] = new_passenger;
                center->numPassengers++;
            
                pthread_mutex_unlock(&center->lock);
            
                // Forward the passenger pointer to the visualizer
                enqueue_message(visualizerQueue, CREATE_PASSENGER, 0, 0, 0, 0, new_passenger);
                break;

            case STATUS_REQUEST:
                pthread_mutex_lock(&center->lock);
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
                    pthread_mutex_unlock(&center->lock);
                    break;
                }

                // Allocate and initialize a new taxi
                Taxi* new_taxi = malloc(sizeof(Taxi));
                if (!new_taxi) {
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
                new_taxi->drop_processed = false;
                pthread_cond_init(&new_taxi->drop_cond, NULL);
                pthread_mutex_init(&new_taxi->lock, NULL);
                init_queue(&new_taxi->queue);

                // Create the taxi thread
                new_taxi->thread_id = create_taxi_thread(new_taxi);
                // Store the taxi in the array
                center->taxis[center->numTaxis] = new_taxi;
                center->numTaxis++;

                pthread_mutex_unlock(&center->lock);
                break;
            }

            case DESTROY_TAXI: {
                pthread_mutex_lock(&center->lock);

                if (center->numTaxis <= 0) {
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
                pthread_cond_destroy(&taxi_to_destroy->drop_cond);
                cleanup_queue(&taxi_to_destroy->queue); // Free all messages in the queue
                free(taxi_to_destroy);

                // Remove the taxi from the array and shift remaining taxis
                for (int i = taxi_index_to_destroy; i < center->numTaxis - 1; i++) {
                    center->taxis[i] = center->taxis[i + 1];
                    center->taxis[i]->id = i + 1; // Update the ID to maintain consistency
                }
                center->taxis[center->numTaxis - 1] = NULL;
                center->numTaxis--;

                pthread_mutex_unlock(&center->lock);
                break;
            }

            case RESET_MAP: {
                pthread_mutex_lock(&center->lock);

                // Send EXIT to all taxis
                for (int i = 0; i < center->numTaxis; i++) {
                    Taxi* taxi = center->taxis[i];
                    if (taxi != NULL) {
                        priority_enqueue_message(&taxi->queue, DROP, 0, 0, 0, 0, NULL);
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
                        pthread_cond_destroy(&taxi->drop_cond);
                        cleanup_queue(&taxi->queue);
                        free(taxi);
                    }
                }

                // Reset the taxi array and counter
                center->numTaxis = 0;
                for (int i = 0; i < MAX_TAXIS; i++) {
                    center->taxis[i] = NULL;
                }

                // Reset the passenger array and counter
                for (int i = 0; i < center->numPassengers; i++) {
                    Passenger* passenger = center->passengers[i];
                    if (passenger != NULL) {
                        free(passenger);
                    }
                }
                center->numPassengers = 0;
                for (int i = 0; i < MAX_PASSENGERS; i++) {
                    center->passengers[i] = NULL;
                }
                pthread_mutex_unlock(&center->lock);

                
                // Forward the RESET_MAP command to the visualizer
                enqueue_message(visualizerQueue, RESET_MAP, 0, 0, 0, 0, NULL);
                break;
            }

            case RANDOM_REQUEST:
                pthread_mutex_lock(&center->lock);
                pthread_mutex_unlock(&center->lock);

                // Forward the message to the visualizer
                enqueue_message(visualizerQueue, RANDOM_REQUEST, msg->data_x, msg->data_y, msg->extra_x, 0, NULL);

                break;

            case ROUTE_PLAN: {

                // Extract the PathData structure from the message
                PathData* path_data = (PathData*)msg->pointer;

                if (path_data) {
                    if (path_data->tamanho_solucao == 0) {
                        // Pathfinding failed, send FINISH to the taxi

                        // Find the taxi by ID
                        pthread_mutex_lock(&center->lock);
                        Taxi* taxi = NULL;
                        for (int j = 0; j < center->numTaxis; j++) {
                            if (center->taxis[j]->id == msg->extra_x) {
                                taxi = center->taxis[j];
                                break;
                            }
                        }
                        pthread_mutex_unlock(&center->lock);

                        if (taxi) {
                            enqueue_message(&taxi->queue, FINISH, 0, 0, 0, 0, NULL);
                        }
                    } else {
                        // Pathfinding succeeded, send MOVE_TO messages
                        // Find the taxi by ID
                        pthread_mutex_lock(&center->lock);
                        Taxi* taxi = NULL;
                        for (int j = 0; j < center->numTaxis; j++) {
                            if (center->taxis[j]->id == msg->extra_x) {
                                taxi = center->taxis[j];
                                break;
                            }
                        }
                        pthread_mutex_unlock(&center->lock);

                        if (taxi) {
                            pthread_mutex_lock(&taxi->lock);
                            taxi->drop_processed = false;
            
                            priority_enqueue_message(&taxi->queue, DROP, 0, 0, 0, 0, NULL);
                            while (!taxi->drop_processed) {
                                pthread_cond_wait(&taxi->drop_cond, &taxi->lock);
                            }
                            pthread_mutex_unlock(&taxi->lock);

                            if(msg->extra_y != 0) {
                                taxi->isFree = false;
                                taxi->currentPassenger = msg->extra_y;
                            }
                            
                            // Iterate through the path and send MOVE_TO messages to the taxi
                            for (int i = 1; i < path_data->tamanho_solucao; i++) {
                                int next_x = path_data->solucaoX[i];
                                int next_y = path_data->solucaoY[i];
                                enqueue_message(&taxi->queue, MOVE_TO, next_x, next_y, 0, 0, NULL);
                            }

                            // Send a FINISH message to the taxi after completing the route
                            enqueue_message(&taxi->queue, FINISH, 0, 0, 0, 0, NULL);
                            if(msg->extra_y != 0) {
                                enqueue_message(&taxi->queue, GOT_PASSENGER, 0, 0, 0, 0, NULL);
                            }
                        }
                    }

                    // Free the PathData structure
                    free(path_data->solucaoX);
                    free(path_data->solucaoY);
                    free(path_data);
                }

                break;
            }

            case GOT_PASSENGER:
            case ARRIVED_AT_DESTINATION: {
                int passenger_id = msg->data_x % R_PASSENGER; // Extract the passenger ID from the message
                bool isDestination = (msg->type == ARRIVED_AT_DESTINATION); // Check if it's the destination

                pthread_mutex_lock(&center->lock);

                // Find the passenger in the vector
                Passenger* passenger = NULL;
                for (int i = 0; i < center->numPassengers; i++) {
                    if (center->passengers[i] && center->passengers[i]->id == passenger_id) {
                        passenger = center->passengers[i];
                        break;
                    }
                }

                if (passenger) {
                    if (isDestination) {

                        // Send ARRIVED_AT_DESTINATION to the visualizer for the destination
                        enqueue_message(center->visualizerQueue, DELETE_PASSENGER,
                                        passenger->x_sidewalk_dest, passenger->y_sidewalk_dest,
                                        passenger->x_road_dest, passenger->y_road_dest, NULL);

                        // Free the passenger memory and remove from the vector
                        for (int i = 0; i < center->numPassengers; i++) {
                            if (center->passengers[i] == passenger) {
                                free(center->passengers[i]);
                                center->passengers[i] = NULL;

                                // Shift remaining passengers in the vector
                                for (int j = i; j < center->numPassengers - 1; j++) {
                                    center->passengers[j] = center->passengers[j + 1];
                                }
                                center->passengers[center->numPassengers - 1] = NULL;
                                center->numPassengers--;
                                break;
                            }
                        }
                    } else {

                        // Send GOT_PASSENGER to the visualizer for the passenger
                        enqueue_message(center->visualizerQueue, DELETE_PASSENGER,
                                        passenger->x_sidewalk, passenger->y_sidewalk,
                                        passenger->x_road, passenger->y_road, NULL);
                    }
                }

                pthread_mutex_unlock(&center->lock);
                break;
            }
            case REFRESH_PASSENGERS:
                refresh_passengers(center);
                break;  
            
            case EXIT_PROGRAM:
                pthread_mutex_lock(&center->lock);

                // Send EXIT message to all taxis with priority
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
                        pthread_cond_destroy(&taxi->drop_cond);
                        cleanup_queue(&taxi->queue);
                        free(taxi);
                    }
                }

                center->numTaxis = 0; // Reset the taxi count

                // Reset the passenger array and counter
                for (int i = 0; i < center->numPassengers; i++) {
                    Passenger* passenger = center->passengers[i];
                    if (passenger != NULL) {
                        free(passenger);
                    }
                }
                center->numPassengers = 0;
                for (int i = 0; i < MAX_PASSENGERS; i++) {
                    center->passengers[i] = NULL;
                }
                // Send EXIT message to the visualizer thread
                enqueue_message(visualizerQueue, EXIT, 0, 0, 0, 0, NULL);
                pthread_mutex_unlock(&center->lock);

                free(msg);
                return NULL;

            default:
                break;
        }

        free(msg);
    }

    return NULL;
}

// Visualizer thread
void* visualizer_thread(void* arg) {
    Visualizer* visualizer = (Visualizer*)arg;

    // Create the map
    Map* map = createMap();
    if (!map) return NULL;

    // Generate the map
    generateMap(map, visualizer->numSquares, visualizer->roadWidth, visualizer->borderWidth,
              visualizer->minSize, visualizer->maxSize, visualizer->minDistance);

    // Print the map after generation
    printLogicalMap(map);
    renderMap(map); // TODO: DEIXAR APENAS O RENDER DEPOIS
    while (1) {
        // Dequeue a message
        Message* msg = dequeue_message(&visualizer->queue);

        switch (msg->type) {
            
            case RANDOM_REQUEST: {
                // Save the taxi's current position and ID
                int taxi_x = msg->data_x;
                int taxi_y = msg->data_y;
                int taxi_id = msg->extra_x;

                // Find a random free point on the map
                int random_x, random_y;
                if (!find_random_free_point(map, &random_x, &random_y)) {
                    break;
                }

                // Create vectors to store the solution path
                int* solutionX = malloc(10000 * sizeof(int));
                int* solutionY = malloc(10000 * sizeof(int));
                int solution_size = 0;

                // Find the path using findPathCoordinates
                if (findPathCoordinates(taxi_x, taxi_y, random_x, random_y, map->matrix, map->cols, map->rows,
                                               solutionX, solutionY, &solution_size) == 0) {
                    // Pathfinding succeeded
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = solutionX;
                    path_data->solucaoY = solutionY;
                    path_data->tamanho_solucao = solution_size;

                    // Send the ROUTE_PLAN message to the control center
                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, 0, path_data);
                } else {
                    // Pathfinding failed

                    // Create a PathData structure with path length 0
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = NULL;
                    path_data->solucaoY = NULL;
                    path_data->tamanho_solucao = 0;

                    // Send the ROUTE_PLAN message to the control center
                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, 0, path_data);

                    // Free the allocated memory for the solution arrays
                    free(solutionX);
                    free(solutionY);
                }

                break;
            }

            case CREATE_PASSENGER: {
            
                // Ensure the map matrix is valid
                if (!map || !map->matrix) {
                    break;
                }
            
                Passenger* passenger = (Passenger*)msg->pointer;
            
                // Check if additional arguments are passed
                if (msg->data_x != 0 || msg->data_y != 0 || msg->extra_x != 0 || msg->extra_y != 0) {
                    // Use the provided coordinates
                    passenger->x_road = msg->data_x;
                    passenger->y_road = msg->data_y;
                    passenger->x_sidewalk = msg->extra_x;
                    passenger->y_sidewalk = msg->extra_y;
            
                    // Use the existing destination coordinates
                    passenger->x_sidewalk_dest = passenger->x_sidewalk_dest;
                    passenger->y_sidewalk_dest = passenger->y_sidewalk_dest;
                    passenger->x_road_dest = passenger->x_road_dest;
                    passenger->y_road_dest = passenger->y_road_dest;
                } else {
                    // Find a random free position adjacent to a SIDEWALK
                    int free_x, free_y, sidewalk_x, sidewalk_y;
                    if (!find_random_free_point_adjacent_to_sidewalk(map, &free_x, &free_y, &sidewalk_x, &sidewalk_y)) {
                        break;
                    }
                    passenger->x_sidewalk = sidewalk_x;
                    passenger->y_sidewalk = sidewalk_y;
                    passenger->x_road = free_x;
                    passenger->y_road = free_y;
            
                    // Find a random free position for the destination
                    int dest_x, dest_y, dest_sidewalk_x, dest_sidewalk_y;
                    if (!find_random_free_point_adjacent_to_sidewalk(map, &dest_x, &dest_y, &dest_sidewalk_x, &dest_sidewalk_y)) {
                        break;
                    }
                    passenger->x_sidewalk_dest = dest_sidewalk_x;
                    passenger->y_sidewalk_dest = dest_sidewalk_y;
                    passenger->x_road_dest = dest_x;
                    passenger->y_road_dest = dest_y;
            
                }
            
                // Add the passenger to the SIDEWALK
                map->matrix[passenger->y_sidewalk][passenger->x_sidewalk] = passenger->id + R_PASSENGER;
                map->matrix[passenger->y_road][passenger->x_road] = passenger->id + R_PASSENGER_POINT;
            
                // Add the destination to the SIDEWALK if it's a new passenger
                if (msg->data_x == 0 && msg->data_y == 0 && msg->extra_x == 0 && msg->extra_y == 0) {
                    map->matrix[passenger->y_sidewalk_dest][passenger->x_sidewalk_dest] = passenger->id + R_PASSENGER_DEST;
                }
            
                // Render the updated map
                renderMap(map);
            
                // Find a free taxi for the passenger
                int* solutionX = malloc(10000 * sizeof(int));
                int* solutionY = malloc(10000 * sizeof(int));
                int solution_size = 0;
            
                if (findPath(passenger->x_road, passenger->y_road, map->matrix, map->cols, map->rows,
                                    solutionX, solutionY, &solution_size, R_TAXI_FREE) == 0) {
                    // Found a free taxi
                    int taxi_x = solutionX[solution_size - 1];
                    int taxi_y = solutionY[solution_size - 1];
            
                    // Create a small vector for destination coordinates
                    int* destinations = malloc(4 * sizeof(int));
                    destinations[0] = passenger->x_road_dest;
                    destinations[1] = passenger->y_road_dest;
                    destinations[2] = passenger->x_sidewalk_dest;
                    destinations[3] = passenger->y_sidewalk_dest;
            
                    // Send the path to the control center
                    enqueue_message(&visualizer->queue, PATHFIND_REQUEST, taxi_x, taxi_y, passenger->x_road, passenger->y_road, destinations);
                }
            
                free(solutionX);
                free(solutionY);
                break;
            }

            case RESET_MAP: {

                // Ensure the map is valid
                if (!map || !map->matrix) {
                    break;
                }

                // Deallocate the old map
                freeMap(map);

                // Create a new map
                map = createMap();
                if (!map) {
                    break;
                }

                // Regenerate the map
                generateMap(map, visualizer->numSquares, visualizer->roadWidth, visualizer->borderWidth,
                          visualizer->minSize, visualizer->maxSize, visualizer->minDistance);

                // Print the new map
                printLogicalMap(map);
                renderMap(map);
                break;
            }
            
            case SPAWN_TAXI: {

                // Garantir que o mapa está válido
                if (!map || !map->matrix) {
                    break;
                }

                // Calcular um ponto aleatório no mapa
                int random_x, random_y;

                if (!find_random_free_point(map, &random_x, &random_y)) {
                    break;
                }

                // Obter o ponteiro para a fila do táxi a partir do campo pointer
                MessageQueue* taxi_queue = (MessageQueue*)msg->pointer;

                // Enviar SPAWN_TAXI para a fila do táxi com as coordenadas calculadas
                enqueue_message(taxi_queue, SPAWN_TAXI, random_x, random_y, 0, 0, NULL);
                // Send FINISH to the taxi after it spawns
                enqueue_message(taxi_queue, FINISH, 0, 0, 0, 0, NULL);
                break;
            }

            case MOVE_TO: {
            
                // Ensure the map is valid
                if (!map || !map->matrix) {
                    break;
                }
                 // Lock the map for writing

                // Handle the special case where the taxi is exiting
                if (msg->extra_x == -1 && msg->extra_y == -1) {
                    // Remove the taxi from the map
                    if (msg->data_x >= 0 && msg->data_y >= 0) {
                        pthread_mutex_lock(&map->lock);
                        map->matrix[msg->data_y][msg->data_x] = ROAD; // Clear the old position
                        pthread_mutex_unlock(&map->lock);
                    }
                    renderMap(map);
                    break;
                }
                
                Taxi* taxi = (Taxi*)msg->pointer;
                int taxi_id = taxi->id;
                bool taxi_isFree = taxi->isFree;
                // Update the map: move the taxi
                pthread_mutex_lock(&map->lock);

                map->matrix[msg->extra_y][msg->extra_x] = taxi_id+(taxi_isFree? R_TAXI_FREE : R_TAXI_OCCUPIED); // Place the taxi in the new position
                if (msg->data_x >= 0 && msg->data_y >= 0) { // Check if the old position is valid
                    map->matrix[msg->data_y][msg->data_x] = ROAD; // Clear the old position
                }
                pthread_mutex_unlock(&map->lock);


                // Render the updated map
                renderMap(map);
                break;
            }

            case PATHFIND_REQUEST: {
            
                // Ensure the map is valid
                if (!map || !map->matrix) {
                    break;
                }
            
                int taxi_x = msg->data_x;
                int taxi_y = msg->data_y;
                int passenger_x = msg->extra_x;
                int passenger_y = msg->extra_y;
            
                int taxi_id = map->matrix[taxi_y][taxi_x];
                taxi_id = taxi_id % R_TAXI_FREE; // Remove the last digit to get the taxi ID
            
                int passenger_id = map->matrix[passenger_y][passenger_x];
                passenger_id = passenger_id % R_PASSENGER_POINT; // Remove the last digit to get the passenger ID                                      
            
                // Allocate memory for the first solution path (taxi to passenger)
                int* solutionX1 = malloc(10000 * sizeof(int));
                int* solutionY1 = malloc(10000 * sizeof(int));
                int solution_size1 = 0;
            
                // Find the path from taxi to passenger
                if (findPathCoordinates(taxi_x, taxi_y, passenger_x, passenger_y, map->matrix, map->cols, map->rows,
                                               solutionX1, solutionY1, &solution_size1) != 0) {
                    free(solutionX1);
                    free(solutionY1);
                    break;
                }
            
                // Check if the pointer is set (indicating a destination exists)
                if (msg->pointer != NULL) {
                    int* destination_coords = (int*)msg->pointer;
                    int dest_x = destination_coords[0];
                    int dest_y = destination_coords[1];
            
                    // Allocate memory for the second solution path (passenger to destination)
                    int* solutionX2 = malloc(10000 * sizeof(int));
                    int* solutionY2 = malloc(10000 * sizeof(int));
                    int solution_size2 = 0;
            
                    // Find the path from passenger to destination
                    if (findPathCoordinates(passenger_x, passenger_y, dest_x, dest_y, map->matrix, map->cols, map->rows,
                                                   solutionX2, solutionY2, &solution_size2) != 0) {
                        free(solutionX1);
                        free(solutionY1);
                        free(solutionX2);
                        free(solutionY2);
                        free(destination_coords);
                        break;
                    }
            
                    // Combine the two paths into a single path with dummy coordinates
                    int total_size = solution_size1 + solution_size2 + 2; // +2 for the dummy coordinates
                    int* combinedX = malloc(total_size * sizeof(int));
                    int* combinedY = malloc(total_size * sizeof(int));
            
                    memcpy(combinedX, solutionX1, solution_size1 * sizeof(int));
                    memcpy(combinedY, solutionY1, solution_size1 * sizeof(int));
            
                    // Add the dummy coordinate (-2, -2) to indicate arrival at the passenger
                    combinedX[solution_size1] = -2;
                    combinedY[solution_size1] = -2;
            
                    memcpy(combinedX + solution_size1 + 1, solutionX2, solution_size2 * sizeof(int));
                    memcpy(combinedY + solution_size1 + 1, solutionY2, solution_size2 * sizeof(int));
            
                    // Add the dummy coordinate (-3, -3) to indicate arrival at the destination
                    combinedX[total_size - 1] = -3;
                    combinedY[total_size - 1] = -3;
            
                    // Free the individual paths
                    free(solutionX1);
                    free(solutionY1);
                    free(solutionX2);
                    free(solutionY2);
                    free(destination_coords);
            
                    // Create a single PathData structure for the combined path
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = combinedX;
                    path_data->solucaoY = combinedY;
                    path_data->tamanho_solucao = total_size;
            
                    // Send the combined ROUTE_PLAN message to the control center
                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, passenger_id, path_data);
            
                } else {
                    // If no destination exists, send only the taxi-to-passenger path
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = solutionX1;
                    path_data->solucaoY = solutionY1;
                    path_data->tamanho_solucao = solution_size1;
            
                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, passenger_id, 0, path_data);
                }
            
                break;
            }
            
            case DELETE_PASSENGER: {
            
                // Ensure the map is valid
                if (!map || !map->matrix) {
                    break;
                }
                
                // Extract the coordinates from the message
                int sidewalk_x = msg->data_x;
                int sidewalk_y = msg->data_y;
                int road_x = msg->extra_x;
                int road_y = msg->extra_y;
                pthread_mutex_lock(&map->lock);
                // Remove the passenger from the map
                map->matrix[sidewalk_y][sidewalk_x] = SIDEWALK; // Clear the SIDEWALK position
                //map->matrix[road_y][road_x] = ROAD;        // Clear the ROAD position
                pthread_mutex_unlock(&map->lock); 
                // Render the updated map
                renderMap(map);
                break;
            }
            case PRINT_LOGICO:
                printLogicalMap(map); // Print the logical map
                break;

            case EXIT:
                freeMap(map);
                free(msg);
                return NULL;

            default:
                break;
        }

        free(msg);
    }
}

// Taxi Thread
void* taxi_thread(void* arg) {
    Taxi* taxi = (Taxi*)arg;

    enqueue_message(taxi->visualizerQueue, SPAWN_TAXI, taxi->x, taxi->y, 0, 0, &taxi->queue);
    while (1) {
        pthread_mutex_lock(&pause_mutex);
        while (isPaused) {
            pthread_cond_wait(&pause_cond, &pause_mutex);
        }
        pthread_mutex_unlock(&pause_mutex);
        // Dequeue a message
        Message* msg = dequeue_message(&taxi->queue);

        // Process the message
        switch (msg->type) {
            case DROP:

                // Clear all messages in the taxi's queue
                cleanup_queue(&taxi->queue);
            
                // Signal the Control Center that DROP has been processed
                pthread_mutex_lock(&taxi->lock);
                taxi->drop_processed = true;
                pthread_cond_signal(&taxi->drop_cond);
                pthread_mutex_unlock(&taxi->lock);

                break;
            case SPAWN_TAXI: 
            
                // Enviar a mensagem MOVE_TO para o visualizador
                enqueue_message(taxi->visualizerQueue, MOVE_TO, 
                                taxi->x, taxi->y, msg->data_x, msg->data_y, taxi);
            
                // Atualizar a posição do táxi para o destino
                pthread_mutex_lock(&taxi->lock);
                taxi->x = msg->data_x;
                taxi->y = msg->data_y;
                pthread_mutex_unlock(&taxi->lock);
            
                break;
            
            case MOVE_TO: 
            
                if (msg->data_x == -2 && msg->data_y == -2) {
                    // Dummy coordinate indicating arrival at the passenger
                    enqueue_message(taxi->control_queue, GOT_PASSENGER, taxi->currentPassenger, 0, 0, 0, NULL);
                    break;
                }
            
                if (msg->data_x == -3 && msg->data_y == -3) {
                    // Dummy coordinate indicating arrival at the destination
                    enqueue_message(taxi->control_queue, ARRIVED_AT_DESTINATION, taxi->currentPassenger, 1, 0, 0, NULL);
                    break;
                }
            
                usleep(TAXI_REFRESH_RATE * (1 + (taxi->isFree * TAXI_SPEED_FACTOR))); 
            
                pthread_mutex_lock(&taxi->lock);
                int old_x = taxi->x;
                int old_y = taxi->y;
                taxi->x = msg->data_x;
                taxi->y = msg->data_y;
                pthread_mutex_unlock(&taxi->lock);
            
                enqueue_message(taxi->visualizerQueue, MOVE_TO, old_x, old_y, msg->data_x, msg->data_y, taxi);
                break;
                              
            case GOT_PASSENGER:
                usleep(TAXI_REFRESH_RATE);
                enqueue_message(taxi->control_queue, GOT_PASSENGER, taxi->currentPassenger, 0, 0, 0, NULL);
                break;

            case FINISH:
                usleep(1000000);
                // Send RANDOM_REQUEST to the control center
                taxi->isFree = true;
                enqueue_message(taxi->control_queue, RANDOM_REQUEST, taxi->x, taxi->y, taxi->id, 0, NULL);

                break;   

            case EXIT:
                if (msg->data_x == 1) {
                } else {
                    enqueue_message(taxi->visualizerQueue, MOVE_TO, taxi->x, taxi->y, -1, -1, NULL);
                }
                free(msg);
                return NULL;

            case STATUS_REQUEST:
                break;

            default:
                break;
        }
        free(msg);
    }
}

// Time Thread
void* timer_thread(void* arg) {
    ControlCenter* center = (ControlCenter*)arg;

    while (1) {
        pthread_mutex_lock(&pause_mutex);
        while (isPaused) {
            pthread_cond_wait(&pause_cond, &pause_mutex);
        }
        pthread_mutex_unlock(&pause_mutex);
        sleep(REFRESH_PASSENGERS_SEC); // Wait for 10 seconds (adjust as needed)

        enqueue_message(&center->queue, REFRESH_PASSENGERS, 0, 0, 0, 0, NULL);
    }

    return NULL;
}

// Create a taxi thread
pthread_t create_taxi_thread(Taxi* taxi) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, taxi_thread, taxi) != 0) {
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
    pthread_t inputThread, controlCenterThread, visualizerThread, timerThread;
    pthread_create(&inputThread, NULL, input_thread, &center);
    pthread_create(&controlCenterThread, NULL, control_center_thread, &center);
    pthread_create(&visualizerThread, NULL, visualizer_thread, &visualizer);
    pthread_create(&timerThread, NULL, timer_thread, &center); // Start the timer thread

    // Wait for threads to finish
    pthread_join(inputThread, NULL);
    pthread_join(controlCenterThread, NULL);
    pthread_join(visualizerThread, NULL);
    pthread_cancel(timerThread);
    pthread_join(timerThread, NULL); // Wait for the timer thread to finish

    // Clean up
    pthread_mutex_destroy(&center.lock);
    pthread_mutex_destroy(&center.queue.lock);
    pthread_cond_destroy(&center.queue.cond);
    pthread_mutex_destroy(&visualizer.queue.lock);
    pthread_cond_destroy(&visualizer.queue.cond);
    cleanup_queue(&center.queue);
    cleanup_queue(&visualizer.queue);

}

int main() {
    init_operations();
    return 0;
}