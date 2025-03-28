#include "mapa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>


//DEFININDO FUNÇÃO DE MAX E MIN
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

//DEFINIÇÃO DE LIMITES DE TENTATIVAS
#define MAX_TENTATIVAS 1000

// DEFINIÇÕES DA MATRIZ MAPA
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

typedef struct
{
    int x;
    int y;
    int parent_index;
} Node;

typedef struct
{
    int x, y;
    int tamanho;
} Quadrado;

int encontraCaminho(int col_inicio, int lin_inicio, char **maze, int num_colunas, int num_linhas,
                    int solucaoCol[], int solucaoLin[], int *tamanho_solucao)
{
    // Fila para BFS
    Node *fila = (Node *)malloc(num_colunas * num_linhas * sizeof(Node));
    int inicio = 0, fim = 0;

    // Matriz de visitados
    int **visitado = (int **)malloc(num_linhas * sizeof(int *));
    for (int lin = 0; lin < num_linhas; lin++)
    {
        visitado[lin] = (int *)malloc(num_colunas * sizeof(int));
        for (int col = 0; col < num_colunas; col++)
        {
            visitado[lin][col] = -1;
        }
    }

    // Adiciona o nó inicial
    fila[fim] = (Node){
        .x = col_inicio,
        .y = lin_inicio,
        .parent_index = -1};
    visitado[lin_inicio][col_inicio] = 0;
    fim++;

    // Delta para as 4 direções (coluna, linha)
    int delta_col[] = {0, 0, -1, 1}; // esquerda, direita
    int delta_lin[] = {-1, 1, 0, 0}; // cima, baixo

    // Executa BFS
    while (inicio < fim)
    {
        Node atual = fila[inicio++];

        // Verifica se é o destino
        if (maze[atual.y][atual.x] == DESTINO)
        {
            // Conta o tamanho do caminho
            int contador = 0;
            int index = inicio - 1;
            while (index != -1)
            {
                contador++;
                index = fila[index].parent_index;
            }

            // Preenche o caminho na ordem correta (origem -> destino)
            *tamanho_solucao = contador;
            index = inicio - 1;
            for (int i = contador - 1; i >= 0; i--)
            {
                solucaoCol[i] = fila[index].x;
                solucaoLin[i] = fila[index].y;
                index = fila[index].parent_index;
            }

            // Libera memória
            for (int lin = 0; lin < num_linhas; lin++)
                free(visitado[lin]);
            free(visitado);
            free(fila);
            return 0;
        }

        // Explora vizinhos
        for (int i = 0; i < 4; i++)
        {
            int nova_col = atual.x + delta_col[i];
            int nova_lin = atual.y + delta_lin[i];

            // Verifica limites
            if (nova_col < 0 || nova_col >= num_colunas ||
                nova_lin < 0 || nova_lin >= num_linhas)
            {
                continue;
            }

            // Verifica se é caminho válido e não visitado
            if ((maze[nova_lin][nova_col] == CAMINHO_LIVRE ||
                 maze[nova_lin][nova_col] == DESTINO) &&
                visitado[nova_lin][nova_col] == -1)
            {

                visitado[nova_lin][nova_col] = inicio - 1;
                fila[fim++] = (Node){
                    .x = nova_col,
                    .y = nova_lin,
                    .parent_index = inicio - 1};
            }
        }
    }

    // Libera memória em caso de falha
    for (int lin = 0; lin < num_linhas; lin++)
        free(visitado[lin]);
    free(visitado);
    free(fila);
    return 1;
}

void marcarCaminho(char **maze, int solucaoX[], int solucaoY[], int tamanho_solucao) // Adicionados parâmetros de tamanho
{
    if (tamanho_solucao <= 0 || maze == NULL || solucaoX == NULL || solucaoY == NULL)
    {
        return;
    }

    for (int i = 0; i < tamanho_solucao - 1; i++)
    {
        int x_atual = solucaoX[i];
        int y_atual = solucaoY[i];
        int x_prox = solucaoX[i + 1];
        int y_prox = solucaoY[i + 1];

        // Determina direção (agora com else if explícito)
        if (x_prox == x_atual + 1 && y_prox == y_atual)
        {
            maze[y_atual][x_atual] = DIREITA; // →
        }
        else if (x_prox == x_atual - 1 && y_prox == y_atual)
        {
            maze[y_atual][x_atual] = ESQUERDA; // ←
        }
        else if (y_prox == y_atual + 1 && x_prox == x_atual)
        {
            maze[y_atual][x_atual] = BAIXO; // ↓
        }
        else if (y_prox == y_atual - 1 && x_prox == x_atual)
        {
            maze[y_atual][x_atual] = CIMA; // ↑
        }
    }

    // Mantém o destino (com validação)
    int last_x = solucaoX[tamanho_solucao - 1];
    int last_y = solucaoY[tamanho_solucao - 1];
    maze[last_y][last_x] = DESTINO;

    return;
}

// Funções auxiliares estáticas (visíveis apenas neste arquivo)
static void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda) {
    // Topo: desenha as bordas superiores (linhas fixas)
    for (int i = 0; i < largura_borda; i++) {
        for (int col = q.x; col < q.x + q.tamanho && col < mapa->colunas; col++) {
            if (q.y + i < mapa->linhas) {
                mapa->matriz[q.y + i][col] = RUA;
            }
        }
    }

    // Base: desenha as bordas inferiores
    for (int i = 0; i < largura_borda; i++) {
        for (int col = q.x; col < q.x + q.tamanho && col < mapa->colunas; col++) {
            if (q.y + q.tamanho - i - 1 < mapa->linhas) {
                mapa->matriz[q.y + q.tamanho - i - 1][col] = RUA;
            }
        }
    }

    // Laterais: desenha as bordas laterais
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

static void encontrarPontosConexao(Quadrado a, Quadrado b,
                                   int *px1, int *py1, // px1 = coluna, py1 = linha
                                   int *px2, int *py2)
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

static void desenharRua(Mapa *mapa, int x1, int y1, int x2, int y2) {
    // x = coluna, y = linha
    
    // Desenha segmento horizontal
    if (x1 != x2) {
        int passo = (x2 > x1) ? 1 : -1;
        for (int x = x1; x != x2; x += passo) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int y = y1 + k - mapa->largura_rua/2;
                if (x >= 0 && x < mapa->colunas && y >= 0 && y < mapa->linhas) {
                    mapa->matriz[y][x] = RUA;
                }
            }
        }
    }

    // Desenha segmento vertical
    if (y1 != y2) {
        int passo = (y2 > y1) ? 1 : -1;
        for (int y = y1; y != y2; y += passo) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int x = x2 + k - mapa->largura_rua/2;
                if (x >= 0 && x < mapa->colunas && y >= 0 && y < mapa->linhas) {
                    mapa->matriz[y][x] = RUA;
                }
            }
        }
    }
}

static void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados) {
    // Verificação de segurança
    if (num_quadrados <= 0 || quadrados == NULL) return;

    int *parent = malloc(num_quadrados * sizeof(int));
    int *key = malloc(num_quadrados * sizeof(int));
    bool *inMST = malloc(num_quadrados * sizeof(bool));

    // Inicialização
    for (int i = 0; i < num_quadrados; i++) {
        key[i] = INT_MAX;
        inMST[i] = false;
    }
    key[0] = 0;
    parent[0] = -1;

    // Algoritmo de Prim
    for (int count = 0; count < num_quadrados - 1; count++) {
        // Encontra o quadrado com menor key
        int min_key = INT_MAX, min_index;
        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v] && key[v] < min_key) {
                min_key = key[v];
                min_index = v;
            }
        }

        inMST[min_index] = true;

        // Atualiza keys dos vizinhos
        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v]) {
                int px1, py1, px2, py2;
                // px = coluna, py = linha
                encontrarPontosConexao(quadrados[min_index], quadrados[v], &px1, &py1, &px2, &py2);
                int distancia = abs(px1 - px2) + abs(py1 - py2);

                if (distancia < key[v]) {
                    parent[v] = min_index;
                    key[v] = distancia;
                }
            }
        }
    }

    // Conecta os quadrados
    for (int i = 1; i < num_quadrados; i++) {
        int px1, py1, px2, py2;
        encontrarPontosConexao(quadrados[parent[i]], quadrados[i], &px1, &py1, &px2, &py2);
        // Garante ordem correta: (x1,y1) -> (x2,y2)
        desenharRua(mapa, px1, py1, px2, py2);
    }

    free(parent);
    free(key);
    free(inMST);
}

// Implementações das funções públicas
Mapa *criarMapa()
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        perror("Erro ao obter tamanho do terminal");
        return NULL;
    }

    Mapa *mapa = malloc(sizeof(Mapa));
    mapa->linhas = ws.ws_row;
    mapa->colunas = ws.ws_col;
    mapa->largura_rua = 1; // Valor padrão

    mapa->matriz = malloc(mapa->linhas * sizeof(char *));
    for (int i = 0; i < mapa->linhas; i++)
    {
        mapa->matriz[i] = malloc(mapa->colunas * sizeof(char));
        for (int j = 0; j < mapa->colunas; j++)
        {
            mapa->matriz[i][j] = CALCADA;
        }
    }

    return mapa;
}

int encontraCaminhoMapa(Mapa *mapa, int inicio_col, int inicio_lin, int dest_col, int dest_lin,
                        int solucaoCol[], int solucaoLin[], int *tamanho_solucao)
{
    int linhas = mapa->linhas, colunas = mapa->colunas;

    // Verificação de limites
    if (inicio_col < 0 || inicio_col >= colunas || inicio_lin < 0 || inicio_lin >= linhas ||
        dest_col < 0 || dest_col >= colunas || dest_lin < 0 || dest_lin >= linhas)
        return 1;

    // Cria cópia da matriz
    char **matriz_copia = malloc(linhas * sizeof(char *));
    for (int i = 0; i < linhas; i++)
    {
        matriz_copia[i] = malloc(colunas * sizeof(char));
        memcpy(matriz_copia[i], mapa->matriz[i], colunas);
    }

    // Marca origem e destino
    matriz_copia[inicio_lin][inicio_col] = ORIGEM; // matriz[linha][coluna]
    matriz_copia[dest_lin][dest_col] = DESTINO;

    // Chama função de busca
    int resultado = encontraCaminho(inicio_col, inicio_lin, matriz_copia, colunas, linhas,
                                    solucaoCol, solucaoLin, tamanho_solucao);

    // Libera memória
    for (int i = 0; i < linhas; i++)
        free(matriz_copia[i]);
    free(matriz_copia);

    return resultado;
}

void gerarMapa(Mapa *mapa, int num_quadrados, int largura_rua, int largura_borda,
               int min_tamanho, int max_tamanho, int distancia_minima)
{
    mapa->largura_rua = largura_rua;
    srand(time(NULL));

    // Verificação de parâmetros (mantido igual)
    if (min_tamanho <= 0 || max_tamanho < min_tamanho || mapa->linhas <= 0 || mapa->colunas <= 0)
    {
        return;
    }

    // Ajuste de tamanho máximo CORRIGIDO:
    int tamanho_max_possivel = MIN(mapa->colunas, mapa->linhas) - 2 * largura_borda; // Trocada ordem
    if (max_tamanho > tamanho_max_possivel)
    {
        max_tamanho = tamanho_max_possivel;
        min_tamanho = MIN(min_tamanho, max_tamanho);
    }

    Quadrado quadrados[num_quadrados];
    int count = 0, tentativas = 0;

    while (count < num_quadrados && tentativas < MAX_TENTATIVAS * num_quadrados)
    {
        int tamanho = rand() % (max_tamanho - min_tamanho + 1) + min_tamanho;

        // Cálculo de limites CORRIGIDO:
        int max_col = mapa->colunas - tamanho - largura_borda; // X (colunas)
        int max_lin = mapa->linhas - tamanho - largura_borda;  // Y (linhas)

        int dist_atual = distancia_minima;
        if (max_col <= 0 || max_lin <= 0)
        {
            dist_atual = 0;
            max_col = mapa->colunas - tamanho;
            max_lin = mapa->linhas - tamanho;
            if (max_col <= 0 || max_lin <= 0)
                break;
        }

        // Criação do quadrado CORRIGIDA:
        Quadrado q = {
            .x = rand() % (max_col + 1), // x = coluna
            .y = rand() % (max_lin + 1), // y = linha
            .tamanho = tamanho};

        // Verificação de colisão (mantida, mas agora com coordenadas corretas)
        bool valido = true;
        for (int i = 0; i < count; i++)
        {
            int dx = abs((q.x + q.tamanho / 2) - (quadrados[i].x + quadrados[i].tamanho / 2));
            int dy = abs((q.y + q.tamanho / 2) - (quadrados[i].y + quadrados[i].tamanho / 2));
            if (dx < dist_atual && dy < dist_atual)
            {
                valido = false;
                break;
            }
        }

        if (valido)
        {
            quadrados[count++] = q;
            desenharQuadrado(mapa, q, largura_borda);
            tentativas = 0;
        }
        else
        {
            tentativas++;
        }
    }

    if (count > 0)
    {
        conectarQuadradosMST(mapa, quadrados, count);
    }
}

void imprimirMapa(Mapa *mapa)
{
    for (int i = 0; i < mapa->linhas; i++)
    {
        for (int j = 0; j < mapa->colunas; j++)
        {
            printf("%c", mapa->matriz[i][j]);
        }
        printf("\n");
    }
}

void desalocarMapa(Mapa *mapa)
{
    if (!mapa)
        return;

    for (int i = 0; i < mapa->linhas; i++)
        free(mapa->matriz[i]);

    free(mapa->matriz);
    free(mapa);
}

