#include "threadsTaxi.h"

// Global variables for pause/play functionality
pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pause_cond = PTHREAD_COND_INITIALIZER;
bool isPaused = false;

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
                    case ' ': // Spacebar to toggle pause/play
                        pthread_mutex_lock(&pause_mutex);
                        isPaused = !isPaused;
                        if (!isPaused) {
                            pthread_cond_broadcast(&pause_cond); // Resume all threads
                            printf("Resuming all threads.\n");
                        } else {
                            printf("Pausing all threads.\n");
                        }
                        pthread_mutex_unlock(&pause_mutex);
                        break;

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

                    case 'l': // Print logical map
                        printf("Key 'l' pressed: Printing logical map.\n");
                        enqueue_message(center->visualizerQueue, PRINT_LOGICO, 0, 0, 0, 0, NULL);
                        break;

                    case 'q': // Quit the program
                        pthread_mutex_lock(&pause_mutex);
                        if (isPaused) {
                            isPaused = false; // Unpause the game
                            pthread_cond_broadcast(&pause_cond); // Resume all threads
                            printf("Resuming all threads before exiting.\n");
                        }
                        pthread_mutex_unlock(&pause_mutex);

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
        pthread_mutex_lock(&pause_mutex);
        while (isPaused) {
            pthread_cond_wait(&pause_cond, &pause_mutex);
        }
        pthread_mutex_unlock(&pause_mutex);
        // Dequeue a message
        Message* msg = dequeue_message(&center->queue);
        //printf("Reading message: %d\n", msg->type);

        // Process the message
        switch (msg->type) {
            case CREATE_PASSENGER:
                pthread_mutex_lock(&center->lock);

                if (center->numPassengers >= MAX_PASSENGERS) {
                    printf("Cannot create more passengers. Maximum limit (%d) reached.\n", MAX_PASSENGERS);
                    pthread_mutex_unlock(&center->lock);
                    break;
                }
            
                // Allocate memory for a new passenger
                Passenger* new_passenger = malloc(sizeof(Passenger));
                if (!new_passenger) {
                    perror("Failed to allocate memory for passenger");
                    pthread_mutex_unlock(&center->lock);
                    break;
                }
            
                // Assign an ID and initialize the passenger
                new_passenger->id = center->numPassengers + 1;
                new_passenger->x_calcada = -1; // Placeholder values
                new_passenger->y_calcada = -1;
                new_passenger->x_rua = -1;
                new_passenger->y_rua = -1;
            
                // Store the passenger in the vector
                center->passengers[center->numPassengers] = new_passenger;
                center->numPassengers++;
            
                printf("Passenger %d created. Total passengers: %d\n", new_passenger->id, center->numPassengers);
            
                pthread_mutex_unlock(&center->lock);
            
                // Forward the passenger pointer to the visualizer
                enqueue_message(visualizerQueue, CREATE_PASSENGER, 0, 0, 0, 0, new_passenger);

                break;

            case CALL_TAXI: {
                pthread_mutex_lock(&center->lock);
                
                // Procura um táxi livre
                Taxi* taxi = NULL;
                for (int i = 0; i < center->numTaxis; i++) {
                    if (center->taxis[i]->id == msg->extra_x && center->taxis[i]->isFree) {
                        taxi = center->taxis[i];
                        break;
                    }
                }
                
                if (taxi) {
                    if (center->numPassengers > 0) {
                        // Tem passageiro esperando
                        Passenger* passenger = center->passengers[0];
                        enqueue_message(center->visualizerQueue, CALL_TAXI,
                                       passenger->x_rua, passenger->y_rua,
                                       passenger->x_calcada, passenger->y_calcada,
                                       passenger);
                    } else {
                        // Não tem passageiro, envia RANDOM_REQUEST
                        printf("No passengers available. Sending RANDOM_REQUEST to Taxi %d\n", taxi->id);
                        enqueue_message(center->visualizerQueue, RANDOM_REQUEST, 
                                       taxi->x, taxi->y, 
                                       taxi->id, 0, taxi);
                    }
                }
                
                pthread_mutex_unlock(&center->lock);
                break;
            }

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
                new_taxi->drop_processed = false;
                pthread_cond_init(&new_taxi->drop_cond, NULL);
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

                pthread_mutex_unlock(&center->lock);

                // Forward the RESET_MAP command to the visualizer
                enqueue_message(visualizerQueue, RESET_MAP, 0, 0, 0, 0, NULL);
                break;
            }

            case RANDOM_REQUEST:
                pthread_mutex_lock(&center->lock);
                //printf("Control Center: Received RANDOM_REQUEST from Taxi ID %d at position (%d, %d).\n", msg->extra_x, msg->data_x, msg->data_y);
                pthread_mutex_unlock(&center->lock);

                Taxi* taxi = NULL;
                // Find the taxi in the vector
                for (int i = 0; i < center->numTaxis; i++)
                {
                    if (center->taxis[i] && center->taxis[i]->id == msg->extra_x)
                    {
                        taxi = center->taxis[i];
                        break;
                    }
                }

                // Forward the message to the visualizer
                enqueue_message(visualizerQueue, RANDOM_REQUEST, msg->data_x, msg->data_y, msg->extra_x, msg->extra_y, taxi);

                break;

                case ROUTE_PLAN: {
                    PathData* path_data = (PathData*)msg->pointer;
                    pthread_mutex_lock(&center->lock);
                    Taxi* taxi = NULL;
                    
                    // Find taxi by ID
                    for (int j = 0; j < center->numTaxis; j++) {
                        if (center->taxis[j]->id == msg->extra_x) {
                            taxi = center->taxis[j];
                            break;
                        }
                    }
                    
                    if (taxi) {
                        // Clear taxi queue
                        pthread_mutex_lock(&taxi->lock);
                        taxi->drop_processed = false;
                        priority_enqueue_message(&taxi->queue, DROP, 0, 0, 0, 0, NULL);
                        while (!taxi->drop_processed) {
                            pthread_cond_wait(&taxi->drop_cond, &taxi->lock);
                        }
                        
                        // Update taxi state based on message
                        if (msg->extra_y != 0) { // Has passenger assignment
                            taxi->isFree = false; // Marcar como ocupado imediatamente
                            taxi->currentPassenger = msg->extra_y;
                            
                            // Find passenger to get destination
                            for (int i = 0; i < center->numPassengers; i++) {
                                if (center->passengers[i]->id == msg->extra_y) {
                                    taxi->destination_x = center->passengers[i]->x_calcada;
                                    taxi->destination_y = center->passengers[i]->y_calcada;
                                    break;
                                }
                            }
                        }
                        pthread_mutex_unlock(&taxi->lock);
                
                        // Send movement commands
                        for (int i = 1; i < path_data->tamanho_solucao; i++) {
                            enqueue_message(&taxi->queue, MOVE_TO, 
                                          path_data->solucaoX[i], 
                                          path_data->solucaoY[i], 
                                          0, 0, NULL);
                        }
                
                        // If carrying passenger, send GOT_PASSENGER when arrives
                        if (msg->extra_y != 0) {
                            enqueue_message(&taxi->queue, GOT_PASSENGER, 0, 0, 0, 0, NULL);
                        } else {
                            enqueue_message(&taxi->queue, FINISH, 0, 0, 0, 0, NULL);
                        }
                    }
                    pthread_mutex_unlock(&center->lock);
                    
                    // Free path data
                    if (path_data) {
                        free(path_data->solucaoX);
                        free(path_data->solucaoY);
                        free(path_data);
                    }
                    break;
                }

                case GOT_PASSENGER:
                {
                    int passenger_id = msg->data_x; // Extract the passenger ID from the message
                    int taxi_id = msg->data_y;      // Extract the taxi ID from the message

                    pthread_mutex_lock(&center->lock);

                    Taxi* taxi = NULL;
                    // Find the taxi in the vector
                    for (int i = 0; i < center->numTaxis; i++)
                    {
                        if (center->taxis[i] && center->taxis[i]->id == taxi_id)
                        {
                            taxi = center->taxis[i];
                            break;
                        }
                    }

                    // Find the passenger in the vector
                    Passenger *passenger = NULL;
                    for (int i = 0; i < center->numPassengers; i++)
                    {
                        if (center->passengers[i] && center->passengers[i]->id == passenger_id)
                        {
                            passenger = center->passengers[i];
                            break;
                        }
                    }

                    if (passenger)
                    {
                        printf("Control Center: Taxi picked up Passenger ID %d at (%d, %d).\n",
                               passenger->id, passenger->x_calcada, passenger->y_calcada);


                        // Send DELETE_PASSENGER to the visualizer
                        enqueue_message(center->visualizerQueue, DELETE_PASSENGER,
                                        passenger->x_calcada, passenger->y_calcada,
                                        passenger->x_rua, passenger->y_rua, NULL);

                        // Remove the passenger from the vector
                        for (int i = 0; i < center->numPassengers; i++)
                        {
                            if (center->passengers[i] == passenger)
                            {
                                free(center->passengers[i]); // Free the passenger memory
                                center->passengers[i] = NULL;

                                // Shift remaining passengers in the vector
                                for (int j = i; j < center->numPassengers - 1; j++)
                                {
                                    center->passengers[j] = center->passengers[j + 1];
                                }
                                center->passengers[center->numPassengers - 1] = NULL;
                                center->numPassengers--;
                                break;
                            }
                        }
                    }
                    else
                    {
                        printf("Control Center: Passenger ID %d not found.\n", passenger_id);
                    }

                    pthread_mutex_unlock(&center->lock);
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
                        pthread_cond_destroy(&taxi->drop_cond);
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
                int taxi_x = msg->data_x;
                int taxi_y = msg->data_y;
                int taxi_id = msg->extra_x;
                int passenger_id = msg->extra_y;

                int dest_x, dest_y;
                Taxi* taxi = (Taxi*) msg->pointer; // Get the taxi pointer from the message

                if (passenger_id != 0) {
                    // This is a passenger destination request
                    find_random_free_point(mapa, &dest_x, &dest_y);
                    taxi->destination_x = dest_x;
                    taxi->destination_y = dest_y;
                } else {
                    // Regular random point request
                    find_random_free_point(mapa, &dest_x, &dest_y);  
                }
            
                // Pathfinding logic remains the same
                int* solucaoX = malloc(10000 * sizeof(int));
                int* solucaoY = malloc(10000 * sizeof(int));
                int tamanho_solucao = 0;
            
                if (encontraCaminhoCoordenadas(taxi_x, taxi_y, dest_x, dest_y, 
                                              mapa->matriz, mapa->colunas, mapa->linhas,
                                              solucaoX, solucaoY, &tamanho_solucao) == 0) {
                    PathData* path_data = malloc(sizeof(PathData));
                    path_data->solucaoX = solucaoX;
                    path_data->solucaoY = solucaoY;
                    path_data->tamanho_solucao = tamanho_solucao;
                    

                    enqueue_message(visualizer->control_queue, ROUTE_PLAN, 
                                   taxi_x, taxi_y, taxi_id, passenger_id, path_data);
                } else {
                    // Handle pathfinding failure
                    free(solucaoX);
                    free(solucaoY);
                }
                break;
            }
            case CALL_TAXI:
            {
                Passenger *passenger = (Passenger *)msg->pointer;

                printf("Visualizer: Finding taxi for passenger at (%d, %d).\n",
                       passenger->x_rua, passenger->y_rua);

                int *solucaoX = malloc(10000 * sizeof(int));
                int *solucaoY = malloc(10000 * sizeof(int));
                int tamanho_solucao = 0;
                
                printf("MENSAGEM ENVIADA1\n %d %d\n", passenger->x_rua, passenger->y_rua);

                // Buscar um taxi próximo
                if (encontraCaminho(passenger->x_rua, passenger->y_rua, mapa->matriz,
                    mapa->colunas, mapa->linhas,
                    solucaoX, solucaoY, &tamanho_solucao, R_TAXI_LIVRE) == 0)
                    {
                        // Enviar PATHFIND_REQUEST com as coordenadas do táxi mais próximo
                        enqueue_message(&visualizer->queue, PATHFIND_REQUEST,
                            solucaoX[tamanho_solucao - 1], solucaoY[tamanho_solucao - 1],
                            passenger->x_rua, passenger->y_rua, NULL);
                            
                        printf("MENSAGEM ENVIADA2\n");
                        }

                free(solucaoX);
                
                free(solucaoY);
                break;
            }

            case CREATE_PASSENGER: {
                printf("Adding a passenger.\n");

                // Ensure the map matrix is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }
                Passenger* passenger = (Passenger*)msg->pointer;

                int free_x, free_y, calcada_x, calcada_y;

                // Find a random free position adjacent to a CALCADA
                if (!find_random_free_point_adjacent_to_calcada(mapa, &free_x, &free_y, &calcada_x, &calcada_y)) {
                    printf("Error: Could not find a valid position for the passenger.\n");
                    break;
                }
                passenger->x_calcada = calcada_x;
                passenger->y_calcada = calcada_y;
                passenger->x_rua = free_x;
                passenger->y_rua = free_y;
                // Add the passenger to the CALCADA
                mapa->matriz[calcada_y][calcada_x] = passenger->id + R_PASSENGER;
                mapa->matriz[free_y][free_x] = passenger->id + R_PONTO_PASSAGEIRO;
                // Render the updated map
                renderMap(mapa);
                
                int* solucaoX = malloc(10000 * sizeof(int));
                int* solucaoY = malloc(10000 * sizeof(int));
                int tamanho_solucao = 0;
                // Buscar um taxi próximo
                encontraCaminho(free_x, free_y,mapa->matriz, mapa->colunas, mapa->linhas,
                                solucaoX, solucaoY, &tamanho_solucao, R_TAXI_LIVRE);

                enqueue_message(&visualizer->queue, PATHFIND_REQUEST, solucaoX[tamanho_solucao - 1], solucaoY[tamanho_solucao - 1], free_x, free_y, NULL);

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

            case MOVE_TO:
            {
                printf("Visualizer: Moving taxi from (%d, %d) to (%d, %d).\n",
                       msg->data_x, msg->data_y, msg->extra_x, msg->extra_y);

                // Ensure the map is valid
                if (!mapa || !mapa->matriz)
                {
                    printf("Error: Map is not initialized.\n");
                    break;
                }

                Taxi *taxi = (Taxi *)msg->pointer;
                int taxi_id = taxi->id;
                bool taxi_isFree = taxi->isFree;

                pthread_mutex_lock(&mapa->lock);

                // Verificar se o táxi permanece no mesmo local
                if (msg->data_x == msg->extra_x && msg->data_y == msg->extra_y)
                {
                    // Atualizar apenas o status do táxi no mapa
                    mapa->matriz[msg->data_y][msg->data_x] = taxi_id + (taxi_isFree ? R_TAXI_LIVRE : R_TAXI_OCUPADO);
                }
                else
                {
                    // Atualizar a posição e o status do táxi no mapa
                    mapa->matriz[msg->extra_y][msg->extra_x] = taxi_id + (taxi_isFree ? R_TAXI_LIVRE : R_TAXI_OCUPADO);
                    if (msg->data_x >= 0 && msg->data_y >= 0)
                    {                                                 // Verificar se a posição antiga é válida
                        mapa->matriz[msg->data_y][msg->data_x] = RUA; // Limpar a posição antiga
                    }
                }

                pthread_mutex_unlock(&mapa->lock);

                // Renderizar o mapa atualizado
                renderMap(mapa);
                break;
            }

            case PATHFIND_REQUEST: {
                printf("Visualizer: Received PATHFIND_REQUEST.\n");

                // Ensure the map is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }
                // Add debugging prints
                printf("Visualizer: Taxi coordinates (%d, %d) to passenger coordinates (%d, %d).\n", msg->data_x, msg->data_y, msg->extra_x, msg->extra_y);
                int taxi_x = msg->data_x;
                int taxi_y = msg->data_y;
                int passenger_x = msg->extra_x;
                int passenger_y = msg->extra_y;

                int taxi_id = mapa->matriz[taxi_y][taxi_x];
                taxi_id = taxi_id % R_TAXI_LIVRE; // Remove the last digit to get the taxi ID

                printf("Visualizer: Taxi ID %d at (%d, %d) requested path to passenger at (%d, %d).\n", taxi_id, taxi_x, taxi_y, passenger_x, passenger_y);
                // Find the path using encontrarCaminhoCoordenadas
                int* solucaoX = malloc(10000 * sizeof(int));
                int* solucaoY = malloc(10000 * sizeof(int));
                int tamanho_solucao = 0;

                // Find the path using encontrarCaminhoCoordenadas
                if (encontraCaminhoCoordenadas(taxi_x, taxi_y, passenger_x, passenger_y, mapa->matriz, mapa->colunas, mapa->linhas,
                    solucaoX, solucaoY, &tamanho_solucao) == 0) {
                // Pathfinding succeeded
                PathData* path_data = malloc(sizeof(PathData));
                path_data->solucaoX = solucaoX;
                path_data->solucaoY = solucaoY;
                path_data->tamanho_solucao = tamanho_solucao;
                int id_passageiro = mapa->matriz[passenger_y][passenger_x] % R_PONTO_PASSAGEIRO; 

                // Send the ROUTE_PLAN message to the control center
                enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, id_passageiro, path_data);

                } else {
                // Pathfinding failed
                //printf("Visualizer: Pathfinding failed for Taxi ID %d. Sending ROUTE_PLAN with path length 0.\n", taxi_id);

                // Create a PathData structure with path length 0
                PathData* path_data = malloc(sizeof(PathData));
                path_data->solucaoX = NULL;
                path_data->solucaoY = NULL;
                path_data->tamanho_solucao = 0;

                // Send the ROUTE_PLAN message to the control center
                enqueue_message(visualizer->control_queue, ROUTE_PLAN, taxi_x, taxi_y, taxi_id, 0, path_data);

                // Free the allocated memory for the solution arrays
                free(solucaoX);
                free(solucaoY);
                }

                break;
            }
            
            case DELETE_PASSENGER: {
                printf("Visualizer: Deleting passenger from map.\n");
            
                // Ensure the map is valid
                if (!mapa || !mapa->matriz) {
                    printf("Error: Map is not initialized.\n");
                    break;
                }
                
                // Extract the coordinates from the message
                int calcada_x = msg->data_x;
                int calcada_y = msg->data_y;
                int rua_x = msg->extra_x;
                int rua_y = msg->extra_y;
                pthread_mutex_lock(&mapa->lock);
                // Remove the passenger from the map
                mapa->matriz[calcada_y][calcada_x] = CALCADA; // Clear the CALCADA position
                //mapa->matriz[rua_y][rua_x] = RUA;        // Clear the RUA position
                pthread_mutex_unlock(&mapa->lock); 
                // Render the updated map
                renderMap(mapa);
                break;
            }
            case PRINT_LOGICO:
                imprimirMapaLogico(mapa); // Print the logical map
                break;

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

// Taxi Thread
void* taxi_thread(void* arg) {
    Taxi* taxi = (Taxi*)arg;

    printf("Taxi %d thread started at position (%d, %d).\n", taxi->id, taxi->x, taxi->y);
    printf("Taxi %d visualizer queue: %p\n", taxi->id, &taxi->visualizerQueue);
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
                printf("Taxi %d received DROP command. Clearing its queue.\n", taxi->id);

                // Clear all messages in the taxi's queue
                cleanup_queue(&taxi->queue);
                
                pthread_mutex_lock(&taxi->lock);
                taxi->isFree = true; // Atualizar o estado para livre
                taxi->currentPassenger = -1; // Reset the current passenger
                taxi->drop_processed = true;
                pthread_cond_signal(&taxi->drop_cond);
                pthread_mutex_unlock(&taxi->lock);

                break;
            case SPAWN_TAXI: 
                printf("Taxi %d received SPAWN_TAXI: Moving to (%d, %d).\n", 
                       taxi->id, msg->data_x, msg->data_y);
            
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
                
                pthread_mutex_unlock(&taxi->lock);

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
            {
                pthread_mutex_lock(&taxi->lock);
                printf("Taxi %d picked up passenger %d\n", taxi->id, taxi->currentPassenger);


                enqueue_message(taxi->control_queue, RANDOM_REQUEST, taxi->x, taxi->y,
                                taxi->destination_x, taxi->destination_y, NULL);

                // Notify control center
                enqueue_message(taxi->control_queue, GOT_PASSENGER,
                                taxi->currentPassenger, taxi->id, 0, 0, NULL);


                break;
                }
                
                case FINISH: {
                    pthread_mutex_lock(&taxi->lock);
                    
                    if (taxi->currentPassenger != -1) {
                        // Just delivered passenger, now available
                        taxi->isFree = true;
                        taxi->currentPassenger = -1;
                        taxi->destination_x = -1;
                        taxi->destination_y = -1;
                        
                        // Request new assignment only if free
                        if (taxi->isFree) {
                            enqueue_message(taxi->control_queue, CALL_TAXI, 
                                           taxi->x, taxi->y, taxi->id, 0, NULL);
                        }
                    } else {
                        // Just moved randomly, remain available
                        taxi->isFree = true;
                        enqueue_message(taxi->control_queue, CALL_TAXI, 
                            taxi->x, taxi->y, taxi->id, 0, NULL);
                    }
                    
                    pthread_mutex_unlock(&taxi->lock);
                    break;
                }

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