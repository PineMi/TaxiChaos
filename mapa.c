#include "mapa.h"

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

    mapa->matriz = malloc(mapa->linhas * sizeof(int*));
    for (int i = 0; i < mapa->linhas; i++) {
        mapa->matriz[i] = malloc(mapa->colunas * sizeof(int));
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


// Draw a square on the map
void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda) {
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

void encontrarPontosConexao(Quadrado a, Quadrado b, int *px1, int *py1, int *px2, int *py2)
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



// Connect squares using MST
void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados) {
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
    } while (mapa->matriz[*random_y][*random_x] != RUA && attempts < max_attempts);

    if (attempts >= max_attempts) {
        printf("Error: Could not find a free position on the map.\n");
        return false;
    }

    return true;
}

// Finds a random free point on the map that is adjacent to a CALCADA
bool find_random_free_point_adjacent_to_calcada(Mapa* mapa, int* free_x, int* free_y, int* calcada_x, int* calcada_y) {
    const int max_attempts = MAX_TENTATIVAS;
    int attempts = 0;

    if (!mapa || !mapa->matriz) {
        printf("Error: Map is not initialized.\n");
        return false;
    }

    do {
        // Generate a random free point
        *free_x = rand() % mapa->colunas;
        *free_y = rand() % mapa->linhas;

        // Check if the point is free (RUA)
        if (mapa->matriz[*free_y][*free_x] == RUA) {
            // Check all adjacent points for a CALCADA
            int delta_x[] = {0, 0, -1, 1};
            int delta_y[] = {-1, 1, 0, 0};

            for (int i = 0; i < 4; i++) {
                int adj_x = *free_x + delta_x[i];
                int adj_y = *free_y + delta_y[i];

                // Ensure the adjacent point is within bounds
                if (adj_x >= 0 && adj_x < mapa->colunas && adj_y >= 0 && adj_y < mapa->linhas) {
                    // Check if the adjacent point is a CALCADA
                    if (mapa->matriz[adj_y][adj_x] == CALCADA) {
                        *calcada_x = adj_x;
                        *calcada_y = adj_y;
                        return true;
                    }
                }
            }
        }

        attempts++;
    } while (attempts < max_attempts);

    printf("Error: Could not find a free position adjacent to a CALCADA.\n");
    return false;
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
            printf("%d", mapa->matriz[i][j]);
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
                default:
                    if (mapa->matriz[i][j] >= R_PASSENGER && mapa->matriz[i][j] < R_PASSENGER + 100) {
                        printf(PASSENGER_EMOJI);
                        break;
                    }
                    if (mapa->matriz[i][j] >= R_TAXI_LIVRE && mapa->matriz[i][j] < R_TAXI_OCUPADO + 100) {
                        printf(TAXI_EMOJI);
                        break;
                    }
                    if (mapa->matriz[i][j] >= R_PONTO_PASSAGEIRO && mapa->matriz[i][j] < R_PONTO_PASSAGEIRO + 100) {
                        printf(PASSENGER_PONTO_EMOJI);
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
int encontraCaminho(int col_inicio, int lin_inicio, int **maze, int num_colunas, int num_linhas,
                    int solucaoCol[], int solucaoLin[], int *tamanho_solucao, int destino) {
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
        if (maze[atual.y][atual.x] >= destino && maze[atual.y][atual.x] < destino + 100) {
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
            if ((maze[nova_lin][nova_col] == RUA ||( maze[nova_lin][nova_col] >= destino && maze[nova_lin][nova_col] < destino + 100)) &&
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
                               int **maze, int num_colunas, int num_linhas,
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
            if ((maze[nova_lin][nova_col] == RUA || (nova_col == col_destino && nova_lin == lin_destino)) &&
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






