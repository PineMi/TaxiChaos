/**
 * UNIVERSIDADE PRESBITERIANA MACKENZIE
 * GRUPO 
 * - BRUNO GERMANETTI RAMALHO - 10426491
 * - MIGUEL CORATOLO SIMÕES PIÑEIRO - 10427085
 * - GABRIEL ERICK MENDES - 10420391
 * - CAMILA NUNES CARNIEL - 10338558
 * - MATEUS TELES MAGALHÃES - 10427410
 * 
 */

/**
 * TAXI SIMULATOR - OVERVIEW
 * 
 * This program simulates a taxi service in a dynamically generated city map. The system includes:
 * - A visual city map with roads, buildings, and sidewalks
 * - Taxis that navigate the city to pick up and drop off passengers
 * - Passengers with random origins and destinations
 * - A control center that coordinates all entities
 * 
 * MAIN COMPONENTS:
 * 1. Map Generation:
 *    - Creates a city map with buildings and connecting roads
 *    - Uses Minimum Spanning Tree algorithm for road connections
 *    - Supports dynamic resizing based on terminal dimensions
 * 
 * 2. Entity System:
 *    - Taxis: Can be created/destroyed dynamically, navigate using pathfinding
 *    - Passengers: Spawn at random locations, request taxis, and have destinations
 *    - Control Center: Manages all entities and coordinates communication
 * 
 * 3. Pathfinding:
 *    - Uses BFS algorithm to find routes between points
 *    - Handles taxi-to-passenger and passenger-to-destination routes
 * 
 * 4. Threading System:
 *    - Separate threads for input, visualization, control, and each taxi
 *    - Message queues for inter-thread communication
 *    - Pause/resume functionality
 * 
 * 5. Visualization:
 *    - Real-time display of the map using emoji characters
 *    - Shows entity positions and status messages
 */

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

#define NUM_SQUARES 10
#define ROAD_WIDTH 2
#define BORDER_WIDTH 2
#define MIN_SIZE 5
#define MAX_SIZE 13
#define MIN_DISTANCE 2

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

#define MAP_VERTICAL_PROPORTION 0.6
#define MAP_HORIZONTAL_PROPORTION 0.5

#define MAX_TAXIS 6

// Global variables for pause/resume functionality and logging
pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pause_cond = PTHREAD_COND_INITIALIZER;
bool isPaused = false;
FILE* log_file = NULL;

// -------------------- STRUCTURES --------------------

/**
 * Node structure for BFS pathfinding algorithm
 * 
 * Represents a single node in the pathfinding grid with:
 * @param x: X-coordinate (column) in the map matrix
 * @param y: Y-coordinate (row) in the map matrix
 * @param parent_index: Index of parent node in BFS queue
 *                       (-1 indicates no parent/starting node)
 */

typedef struct {
    int x;
    int y;
    int parent_index;
} Node;

/**
 * Square structure for map generation
 * 
 * Represents a building block in the city map with:
 * @param x: Top-left X coordinate of the square
 * @param y: Top-left Y coordinate of the square
 * @param size: Width/height of the square (squares are always equal width/height)
 */

typedef struct {
    int x, y;
    int size;
} Square;

/**
 * Map structure containing city layout
 * 
 * Represents the entire city map with:
 * @param rows: Number of rows in the matrix
 * @param cols: Number of columns in the matrix
 * @param road_width: Width of roads in cells
 * @param matrix: 2D array representing map cells
 * @param lock: Mutex for thread-safe map access
 */

typedef struct {
    int rows, cols;
    int road_width;
    int **matrix;
    pthread_mutex_t lock; 
} Map;

/**
 * Passenger structure representing a taxi customer
 * 
 * Contains all information about a passenger including:
 * @param id: Unique passenger identifier
 * @param x_sidewalk: X coordinate of sidewalk pickup point
 * @param y_sidewalk: Y coordinate of sidewalk pickup point
 * @param x_road: X coordinate of adjacent road pickup point
 * @param y_road: Y coordinate of adjacent road pickup point
 * @param isFree: Status flag (unused in current implementation)
 * @param x_sidewalk_dest: X coordinate of sidewalk destination
 * @param y_sidewalk_dest: Y coordinate of sidewalk destination
 * @param x_road_dest: X coordinate of adjacent road destination
 * @param y_road_dest: Y coordinate of adjacent road destination
 */

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

/**
 * Message structure for inter-thread communication
 * 
 * Used in message queues to pass commands between threads with:
 * @param type: Type of message (from MessageType enum)
 * @param next: Pointer to next message in queue
 * @param data_x: Primary X coordinate data
 * @param data_y: Primary Y coordinate data
 * @param extra_x: Secondary X coordinate or ID data
 * @param extra_y: Secondary Y coordinate or flags
 * @param pointer: Generic pointer for additional data
 */

typedef struct Message {
    MessageType type;
    struct Message* next;
    int data_x;
    int data_y;
    int extra_x;
    int extra_y;
    void* pointer;
} Message;

/**
 * Message queue structure for thread communication
 * 
 * Thread-safe FIFO queue implementation with:
 * @param head: Pointer to first message in queue
 * @param tail: Pointer to last message in queue
 * @param lock: Mutex for thread-safe operations
 * @param cond: Condition variable for blocking dequeue
 */

typedef struct {
    Message* head;
    Message* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} MessageQueue;

/**
 * Path data structure for storing navigation solutions
 * 
 * Contains computed path information with:
 * @param solucaoX: Array of X coordinates in path
 * @param solucaoY: Array of Y coordinates in path
 * @param tamanho_solucao: Number of points in path
 */

typedef struct {
    int* solucaoX;         
    int* solucaoY;         
    int tamanho_solucao;   
} PathData;

/**
 * Taxi structure representing a taxi vehicle
 * 
 * Contains all taxi state information including:
 * @param id: Unique taxi identifier
 * @param x: Current X position
 * @param y: Current Y position
 * @param isFree: Availability status (true if available)
 * @param currentPassenger: ID of assigned passenger (-1 if none)
 * @param queue: Message queue for receiving commands
 * @param lock: Mutex for thread-safe operations
 * @param control_queue: Pointer to control center's queue
 * @param visualizerQueue: Pointer to visualizer's queue
 * @param thread_id: POSIX thread identifier
 * @param drop_cond: Condition variable for drop synchronization
 * @param drop_processed: Flag indicating drop completion
 */

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

/**
 * Control center structure for system coordination
 * 
 * Central management point containing:
 * @param numPassengers: Current passenger count
 * @param lock: Mutex for thread-safe operations
 * @param queue: Message queue for receiving commands
 * @param visualizerQueue: Pointer to visualizer's queue
 * @param taxis: Array of pointers to active taxis
 * @param numTaxis: Current taxi count
 * @param passengers: Array of pointers to active passengers
 */

typedef struct {
    int numPassengers;
    pthread_mutex_t lock;
    MessageQueue queue;
    MessageQueue* visualizerQueue; 
    Taxi* taxis[MAX_TAXIS];
    int numTaxis;
    Passenger* passengers[MAX_PASSENGERS]; 
} ControlCenter;

/**
 * Visualizer structure for map rendering
 * 
 * Contains visualization parameters and state with:
 * @param numSquares: Number of buildings in map
 * @param roadWidth: Width of roads in cells
 * @param borderWidth: Width of building borders
 * @param minSize: Minimum building size
 * @param maxSize: Maximum building size
 * @param minDistance: Minimum distance between buildings
 * @param queue: Message queue for receiving commands
 * @param control_queue: Pointer to control center's queue
 * @param center: Pointer to control center structure
 */

typedef struct {
    int numSquares;
    int roadWidth;
    int borderWidth;
    int minSize;
    int maxSize;
    int minDistance;
    MessageQueue queue;
    MessageQueue* control_queue;
    ControlCenter* center;
} Visualizer;

// Function prototypes
static void drawSquare(Map* map, Square q, int borderWidth);
static void connectSquaresMST(Map* map, Square* squares, int num_squares);
static void findConnectionPoints(Square a, Square b, int* px1, int* py1, int* px2, int* py2);
void init_operations();
void renderMap(Map* map, ControlCenter* center, Visualizer* visualizer);
pthread_t create_taxi_thread(Taxi* taxi);
bool find_random_free_point(Map* map, int* random_x, int* random_y);
const char* message_type_to_abbreviation(MessageType type);


// -------------------- QUEUE FUNCTIONS ---------------------

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

    // Log the message
    if (log_file) {
        pthread_mutex_lock(&pause_mutex); // Ensure thread-safe logging
        fprintf(log_file, "Enqueued Message: Type=%s, DataX=%d, DataY=%d, ExtraX=%d, ExtraY=%d\n",
                message_type_to_abbreviation(type), x, y, extra_x, extra_y);
        fflush(log_file); // Ensure the log is written immediately
        pthread_mutex_unlock(&pause_mutex);
    }
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

    if (log_file) {
        pthread_mutex_lock(&pause_mutex); // Ensure thread-safe logging
        fprintf(log_file, "Enqueued Message: Type=%s, DataX=%d, DataY=%d, ExtraX=%d, ExtraY=%d\n",
                message_type_to_abbreviation(type), x, y, extra_x, extra_y);
        fflush(log_file); // Ensure the log is written immediately
        pthread_mutex_unlock(&pause_mutex);
    }
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

/**
 * Creates a new map structure based on terminal dimensions
 * 
 * Dynamically allocates and initializes a map structure by:
 * - Getting terminal dimensions using ioctl
 * - Scaling dimensions by MAP_VERTICAL_PROPORTION and MAP_HORIZONTAL_PROPORTION
 * - Allocating matrix memory
 * - Initializing all cells to SIDEWALK (1)
 * 
 * @return Pointer to newly created Map structure
 * @note Returns NULL if terminal dimensions cannot be obtained
 */

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

/**
 * Frees all memory associated with a map
 * 
 * Safely deallocates map resources by:
 * - Freeing each row of the matrix
 * - Freeing the matrix pointer array
 * - Freeing the map structure itself
 * 
 * @param map Pointer to Map structure to deallocate
 * @note Handles NULL pointer safely
 */

void freeMap(Map* map) {
    if (!map) return;

    for (int i = 0; i < map->rows; i++) {
        free(map->matrix[i]);
    }
    free(map->matrix);
    free(map);
}

/**
 * Generates a city map with buildings and roads
 * 
 * Creates a procedural city layout by:
 * 1. Clearing existing map (setting all cells to SIDEWALK)
 * 2. Generating random building squares with:
 *    - Random positions and sizes within min/max constraints
 *    - Minimum distance between buildings
 *    - Border roads around each building
 * 3. Connecting buildings with roads using MST algorithm
 * 
 * @param map Pointer to Map structure to generate
 * @param num_squares Number of buildings to generate
 * @param road_width Width of roads in cells
 * @param border_width Width of building borders
 * @param min_size Minimum building dimension
 * @param max_size Maximum building dimension
 * @param min_distance Minimum spacing between buildings
 */

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

/**
 * Prints the raw numerical representation of the map
 * 
 * Debug function that displays:
 * - Each cell's numerical value
 * - Matrix layout with rows and columns
 * 
 * @param map Pointer to Map structure to display
 */

void printLogicalMap(Map* map) {
    for (int i = 0; i < map->rows; i++) {
        for (int j = 0; j < map->cols; j++) {
            printf("%d", map->matrix[i][j]);
        }
        printf("\n");
    }
}

const char* message_type_to_abbreviation(MessageType type) {
    switch (type) {
        case CREATE_PASSENGER: return "[CP]";
        case DELETE_PASSENGER: return "[DP]";
        case RESET_MAP: return "[RM]";
        case EXIT_PROGRAM: return "[EP]";
        case PATHFIND_REQUEST: return "[PFR]";
        case RANDOM_REQUEST: return "[RR]";
        case ROUTE_PLAN: return "[RP]";
        case EXIT: return "[EX]";
        case STATUS_REQUEST: return "[SR]";
        case CREATE_TAXI: return "[CT]";
        case DESTROY_TAXI: return "[DT]";
        case SPAWN_TAXI: return "[ST]";
        case MOVE_TO: return "[MOV]";
        case FINISH: return "[FIN]";
        case PRINT_LOGICO: return "[PL]";
        case DROP: return "[DRP]";
        case GOT_PASSENGER: return "[GP]";
        case ARRIVED_AT_DESTINATION: return "[AD]";
        case REFRESH_PASSENGERS: return "[RPAS]";
        default: return "[UNK]";
    }
}

void print_message_queue(const char* thread_name, MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);

    printf("Thread %s:\n", thread_name);

    Message* current = queue->head;
    int count = 0;
    int remaining = 0;

    while (current) {
        if (count < 6) {
            printf("%s", message_type_to_abbreviation(current->type));
        } else {
            remaining++;
        }
        current = current->next;
        count++;
    }

    if (count > 6) {
        printf(" + %d", remaining);
    } else if (count == 0) {
        printf("[EMPTY]");
    }

    printf("\n---------------------------------------\n");

    pthread_mutex_unlock(&queue->lock);
}

void renderMap(Map* map, ControlCenter* center, Visualizer* visualizer) {
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
                        printf(DESTINATION_EMOJI);
                        break;
                    }
                    printf("?"); 
                    break;
            }
        }
        printf("\n");
    }

    printf("\n--- Message Queues ---\n");
    print_message_queue("ControlCenter", &center->queue);
    print_message_queue("Visualizer", &visualizer->queue);
    if (center->numTaxis > 0 && center->taxis[0]) {
        print_message_queue("Taxi 1", &center->taxis[0]->queue);
    }
}

/**
 * Finds a path between two points using BFS algorithm
 * 
 * Calculates shortest path through road network by:
 * 1. Initializing BFS queue and visited matrix
 * 2. Exploring neighbors (up/down/left/right)
 * 3. Tracking parent nodes to reconstruct path
 * 4. Handling special destination ranges (100-599)
 * 
 * @param start_col Starting X coordinate
 * @param start_row Starting Y coordinate
 * @param maze The map matrix to navigate
 * @param num_cols Width of map
 * @param num_rows Height of map
 * @param solutionCol Output array for path X coordinates
 * @param solutionRow Output array for path Y coordinates
 * @param solution_size Output pointer for path length
 * @param destination Target value range (e.g., R_TAXI_FREE)
 * @return 0 on success, 1 if no path found
 */

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

/**
 * Finds path between specific coordinates using BFS
 * 
 * Specialized version of findPath that navigates to exact coordinates
 * rather than destination ranges. Used for taxi-passenger routing.
 * 
 * @param start_col Starting X coordinate
 * @param start_row Starting Y coordinate
 * @param dest_col Destination X coordinate
 * @param dest_row Destination Y coordinate
 * @param maze The map matrix to navigate
 * @param num_cols Width of map
 * @param num_rows Height of map
 * @param solutionCol Output array for path X coordinates
 * @param solutionRow Output array for path Y coordinates
 * @param solution_size Output pointer for path length
 * @return 0 on success, 1 if no path found
 */

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

/**
 * Marks a calculated path on the map matrix
 * 
 * Annotates the path with directional markers (→, ←, ↑, ↓) by:
 * - Analyzing each step's direction
 * - Setting appropriate directional constants
 * - Preserving destination marker
 * 
 * @param maze The map matrix to annotate
 * @param solutionX Array of path X coordinates
 * @param solutionY Array of path Y coordinates
 * @param solution_size Length of path
 */

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

/**
 * Draws a building square on the map with borders
 * 
 * Creates a building by:
 * 1. Drawing top/bottom borders (horizontal roads)
 * 2. Drawing left/right borders (vertical roads)
 * 3. Leaving center area as sidewalk (1)
 * 
 * @param map Pointer to Map structure
 * @param q Square parameters (position, size)
 * @param borderWidth Width of border roads
 */

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

/**
 * Draws a road segment between two points
 * 
 * Creates either horizontal or vertical road segments with:
 * - Proper road width handling
 * - Boundary checking
 * - Center-aligned road placement
 * 
 * @param map Pointer to Map structure
 * @param x1 Starting X coordinate
 * @param y1 Starting Y coordinate
 * @param x2 Ending X coordinate
 * @param y2 Ending Y coordinate
 */

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

/**
 * Connects buildings using Minimum Spanning Tree algorithm
 * 
 * Creates efficient road network by:
 * 1. Calculating building center points
 * 2. Using Prim's algorithm to find MST
 * 3. Drawing roads between connected buildings
 * 
 * @param map Pointer to Map structure
 * @param squares Array of building squares
 * @param num_squares Number of buildings
 */

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

/**
 * Calculates connection points between two buildings
 * 
 * Determines optimal road attachment points by:
 * - Comparing relative positions (horizontal/vertical dominance)
 * - Selecting midpoints on appropriate sides
 * 
 * @param a First building square
 * @param b Second building square
 * @param px1 Output for first point's X
 * @param py1 Output for first point's Y
 * @param px2 Output for second point's X
 * @param py2 Output for second point's Y
 */

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

/**
 * Finds random accessible point on road network
 * 
 * Locates valid spawn points by:
 * - Random sampling with maximum attempts
 * - Checking for ROAD (0) cell type
 * 
 * @param map Pointer to Map structure
 * @param random_x Output for found X coordinate
 * @param random_y Output for found Y coordinate
 * @return true if valid point found, false otherwise
 */

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

/**
 * Finds road point adjacent to sidewalk
 * 
 * Locates passenger pickup/dropoff points by:
 * - Finding road cells (0) next to sidewalk cells (1)
 * - Valid for both passenger origins and destinations
 * 
 * @param map Pointer to Map structure
 * @param free_x Output for road X coordinate
 * @param free_y Output for road Y coordinate
 * @param sidewalk_x Output for adjacent sidewalk X
 * @param sidewalk_y Output for adjacent sidewalk Y
 * @return true if valid point found, false otherwise
 */

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

/**
 * Refreshes passenger positions and re-queues unassigned passengers
 * 
 * Periodically called to:
 * - Check passenger assignment status
 * - Re-queue passengers not assigned to taxis
 * - Maintain active passenger visibility
 * 
 * @param center Pointer to ControlCenter structure
 * @note Called by timer thread at REFRESH_PASSENGERS_SEC intervals
 */

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

/**
 * Handles keyboard input and command generation
 * 
 * Runs in dedicated thread to:
 * - Configure terminal for non-blocking input
 * - Process arrow keys and character commands
 * - Generate appropriate messages for:
 *   * Taxi creation/destruction
 *   * Passenger creation
 *   * Map reset
 *   * Program control
 * - Restores terminal settings on exit
 * 
 * @param arg ControlCenter pointer passed as void*
 * @return NULL on thread exit
 * 
 * @note Uses escape sequence detection for arrow keys
 * @warning Terminal settings are process-global
 */

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

/**
 * Main control center processing thread
 * 
 * Acts as system coordinator by:
 * - Processing messages from input thread
 * - Managing taxi lifecycle (create/destroy)
 * - Handling passenger requests
 * - Routing pathfinding requests
 * - Coordinating system reset
 * - Implementing pause functionality
 * 
 * @param arg ControlCenter pointer passed as void*
 * @return NULL on program exit
 * 
 * @note Implements core message processing state machine
 * @warning Holds locks during taxi thread joins
 */

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

/**
 * Map visualization and rendering thread
 * 
 * Responsible for:
 * - Map generation and maintenance
 * - Real-time display rendering
 * - Processing visual updates from:
 *   * Taxi movements
 *   * Passenger spawns
 *   * Destination markers
 * - Handling pathfinding requests
 * - Maintaining logical map consistency
 * 
 * @param arg Visualizer pointer passed as void*
 * @return NULL on program exit
 * 
 * @note Uses ANSI escape codes for display control
 * @warning Map matrix access requires proper locking
 */

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
    renderMap(map, visualizer->center, visualizer); // TODO: DEIXAR APENAS O RENDER DEPOIS
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
                renderMap(map, visualizer->center, visualizer);
            
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
                renderMap(map, visualizer->center, visualizer);
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
                    renderMap(map, visualizer->center, visualizer);
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
                renderMap(map, visualizer->center, visualizer);
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
                renderMap(map, visualizer->center, visualizer);
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

/**
 * Taxi behavior and navigation thread
 * 
 * Implements taxi agent that:
 * - Processes movement commands
 * - Handles passenger pickup/dropoff
 * - Maintains state (position, availability)
 * - Communicates with control center
 * - Implements movement delay based on:
 *   * TAXI_REFRESH_RATE base speed
 *   * TAXI_SPEED_FACTOR for occupied taxis
 * 
 * @param arg Taxi pointer passed as void*
 * @return NULL on taxi destruction
 * 
 * @note Uses usleep for movement timing
 * @warning Queue cleanup required on exit
 */

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

/**
 * Periodic timer thread for system events
 * 
 * Provides timed events by:
 * - Triggering passenger refresh at fixed intervals
 * - Maintaining operation during pause state
 * - Sleeping between triggers
 * 
 * @param arg ControlCenter pointer passed as void*
 * @return NULL (thread runs indefinitely)
 * 
 * @note Uses sleep() for timing
 * @warning Cancellation point requires cleanup
 */

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

/**
 * Creates and starts a new taxi thread
 * 
 * Wrapper for pthread_create that:
 * - Initializes thread with taxi context
 * - Handles creation errors
 * - Returns thread identifier
 * 
 * @param taxi Pointer to initialized Taxi structure
 * @return pthread_t identifier for new thread
 * 
 * @note Exits program on thread creation failure
 */

pthread_t create_taxi_thread(Taxi* taxi) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, taxi_thread, taxi) != 0) {
        exit(EXIT_FAILURE);
    }
    return thread;
}


// -------------------- MAIN FUNCTION --------------------

/**
 * Initializes and starts the entire taxi simulator system
 * 
 * Performs complete system setup by:
 * 1. Opening log file for operation tracking
 * 2. Initializing control center with:
 *    - Passenger/taxi counters
 *    - Mutex for thread safety
 *    - Message queue for commands
 * 3. Configuring visualizer parameters:
 *    - Map generation constants (squares, sizes, spacing)
 *    - Message queue for display updates
 * 4. Establishing inter-component links:
 *    - Connecting visualizer to control center
 *    - Setting up control center to visualizer feedback
 * 5. Launching core threads:
 *    - Input handling
 *    - Control processing
 *    - Visualization
 *    - Background timing
 * 6. Managing thread lifecycle:
 *    - Proper shutdown sequencing
 *    - Resource cleanup
 * 
 * @note Called once at program startup
 * @warning Log file creation failure terminates program
 */

void init_operations() {
    log_file = fopen("operation_log.txt", "w");
    if (!log_file) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
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
    int numSquares = NUM_SQUARES;
    int roadWidth = ROAD_WIDTH;
    int borderWidth = BORDER_WIDTH;
    int minSize = MIN_SIZE;
    int maxSize = MAX_SIZE;
    int minDistance = MIN_DISTANCE;

    Visualizer visualizer = {numSquares, roadWidth, borderWidth, minSize, maxSize, minDistance};
    visualizer.center = &center;
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
    
    // Close the log file
    if (log_file) {
        fclose(log_file);
    }
}

int main() {
    init_operations();
    return 0;
}