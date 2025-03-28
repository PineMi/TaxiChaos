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

int encontraCaminho(int x_atual, int y_atual, char **maze, int largura, int altura,
                    int solucaoX[], int solucaoY[], int *tamanho_solucao)
{
    // Fila para BFS
    Node *fila = (Node *)malloc(largura * altura * sizeof(Node));
    int inicio = 0, fim = 0;

    // Matriz para rastrear nós visitados e seus pais
    int **visitado = (int **)malloc(altura * sizeof(int *));
    for (int i = 0; i < altura; i++)
    {
        visitado[i] = (int *)malloc(largura * sizeof(int));
        for (int j = 0; j < largura; j++)
        {
            visitado[i][j] = -1; // -1 significa não visitado
        }
    }

    // Adiciona o nó inicial à fila
    fila[fim].x = x_atual;
    fila[fim].y = y_atual;
    fila[fim].parent_index = -1; // Nó inicial não tem pai
    fim++;
    visitado[y_atual][x_atual] = 0; // Primeiro elemento na fila (índice 0)

    // Direções possíveis: cima, baixo, esquerda, direita
    int dx[] = {0, 0, -1, 1};
    int dy[] = {1, -1, 0, 0};

    int destino_x = -1, destino_y = -1;
    int destino_index = -1;

    // Executa BFS
    while (inicio < fim)
    {
        Node atual = fila[inicio];
        inicio++;

        // Verifica se é o destino
        if (maze[atual.y][atual.x] == DESTINO)
        {
            destino_x = atual.x;
            destino_y = atual.y;
            destino_index = inicio - 1;
            break;
        }

        // Explora vizinhos
        for (int i = 0; i < 4; i++)
        {
            int novo_x = atual.x + dx[i];
            int novo_y = atual.y + dy[i];

            // Verifica limites
            if (novo_x < 0 || novo_x >= largura || novo_y < 0 || novo_y >= altura)
            {
                continue;
            }

            // Verifica se é caminho válido e não visitado
            char celula = maze[novo_y][novo_x];
            if ((celula == CAMINHO_LIVRE || celula == DESTINO) && visitado[novo_y][novo_x] == -1)
            {
                visitado[novo_y][novo_x] = inicio - 1; // Armazena o índice do pai
                fila[fim].x = novo_x;
                fila[fim].y = novo_y;
                fila[fim].parent_index = inicio - 1;
                fim++;
            }
        }
    }

    // Se encontrou o destino, reconstrói o caminho
    if (destino_index != -1)
    {
        // Primeiro conta o tamanho do caminho
        int contador = 0;
        int index = destino_index;
        while (index != -1)
        {
            contador++;
            index = fila[index].parent_index;
        }

        // Preenche o caminho na ordem inversa (origem -> destino)
        *tamanho_solucao = contador;
        index = destino_index;
        for (int i = contador - 1; i >= 0; i--)
        {
            solucaoX[i] = fila[index].x;
            solucaoY[i] = fila[index].y;
            index = fila[index].parent_index;
        }

        // Libera memória
        for (int i = 0; i < altura; i++)
        {
            free(visitado[i]);
        }
        free(visitado);
        free(fila);

        return 0;
    }

    // Libera memória em caso de falha
    for (int i = 0; i < altura; i++)
    {
        free(visitado[i]);
    }
    free(visitado);
    free(fila);

    return 1;
}

void marcarCaminho(char **maze, int solucaoX[], int solucaoY[], int tamanho_solucao)
{
    for (int i = 0; i < tamanho_solucao - 1; i++)
    {
        int x_atual = solucaoX[i];
        int y_atual = solucaoY[i];
        int x_prox = solucaoX[i + 1];
        int y_prox = solucaoY[i + 1];

        // Determina a direção para a próxima posição
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

    // Mantém o destino original
    maze[solucaoY[tamanho_solucao - 1]][solucaoX[tamanho_solucao - 1]] = DESTINO;
}

// Funções auxiliares estáticas (visíveis apenas neste arquivo)
static void desenharQuadrado(Mapa *mapa, Quadrado q, int largura_borda)
{
    // Topo
    for (int i = 0; i < largura_borda; i++)
    {
        for (int j = q.y; j < q.y + q.tamanho && j < mapa->colunas; j++)
        {
            if (q.x + i < mapa->linhas)
                mapa->matriz[q.x + i][j] = RUA;
        }
    }

    // Base
    for (int i = 0; i < largura_borda; i++)
    {
        for (int j = q.y; j < q.y + q.tamanho && j < mapa->colunas; j++)
        {
            if (q.x + q.tamanho - i - 1 < mapa->linhas)
                mapa->matriz[q.x + q.tamanho - i - 1][j] = RUA;
        }
    }

    // Laterais
    for (int i = 0; i < largura_borda; i++)
    {
        for (int j = q.x; j < q.x + q.tamanho && j < mapa->linhas; j++)
        {
            if (q.y + i < mapa->colunas)
                mapa->matriz[j][q.y + i] = RUA;
            if (q.y + q.tamanho - i - 1 < mapa->colunas)
                mapa->matriz[j][q.y + q.tamanho - i - 1] = RUA;
        }
    }
}

static void encontrarPontosConexao(Quadrado a, Quadrado b, int *px1, int *py1, int *px2, int *py2)
{
    int pontos_a[4][2] = {
        {a.x + a.tamanho / 2, a.y},                 // Topo
        {a.x + a.tamanho / 2, a.y + a.tamanho - 1}, // Base
        {a.x, a.y + a.tamanho / 2},                 // Esquerda
        {a.x + a.tamanho - 1, a.y + a.tamanho / 2}  // Direita
    };

    int pontos_b[4][2] = {
        {b.x + b.tamanho / 2, b.y},
        {b.x + b.tamanho / 2, b.y + b.tamanho - 1},
        {b.x, b.y + b.tamanho / 2},
        {b.x + b.tamanho - 1, b.y + b.tamanho / 2}};

    int min_dist = INT_MAX;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int dx = pontos_a[i][0] - pontos_b[j][0];
            int dy = pontos_a[i][1] - pontos_b[j][1];
            int dist = dx * dx + dy * dy;

            if (dist < min_dist)
            {
                min_dist = dist;
                *px1 = pontos_a[i][0];
                *py1 = pontos_a[i][1];
                *px2 = pontos_b[j][0];
                *py2 = pontos_b[j][1];
            }
        }
    }
}

static void desenharRua(Mapa *mapa, int x1, int y1, int x2, int y2)
{
    // Desenha segmento horizontal
    if (y1 != y2)
    {
        int passo_y = (y2 > y1) ? 1 : -1;
        for (int y = y1; y != y2; y += passo_y)
        {
            for (int k = 0; k < mapa->largura_rua; k++)
            {
                int x = x1 + k - mapa->largura_rua / 2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas)
                {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }

    // Desenha segmento vertical
    if (x1 != x2)
    {
        int passo_x = (x2 > x1) ? 1 : -1;
        for (int x = x1; x != x2; x += passo_x)
        {
            for (int k = 0; k < mapa->largura_rua; k++)
            {
                int y = y2 + k - mapa->largura_rua / 2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas)
                {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }

    // Preenche o canto entre os segmentos
    if (x1 != x2 && y1 != y2)
    {
        for (int kx = 0; kx < mapa->largura_rua; kx++)
        {
            for (int ky = 0; ky < mapa->largura_rua; ky++)
            {
                int x = x1 + kx - mapa->largura_rua / 2;
                int y = y2 + ky - mapa->largura_rua / 2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas)
                {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }
}

static void conectarQuadradosMST(Mapa *mapa, Quadrado *quadrados, int num_quadrados)
{
    int *parent = malloc(num_quadrados * sizeof(int));
    int *key = malloc(num_quadrados * sizeof(int));
    bool *inMST = malloc(num_quadrados * sizeof(bool));

    for (int i = 0; i < num_quadrados; i++)
    {
        key[i] = INT_MAX;
        inMST[i] = false;
    }

    key[0] = 0;
    parent[0] = -1;

    for (int count = 0; count < num_quadrados - 1; count++)
    {
        int min_key = INT_MAX, min_index;
        for (int v = 0; v < num_quadrados; v++)
        {
            if (!inMST[v] && key[v] < min_key)
            {
                min_key = key[v];
                min_index = v;
            }
        }

        inMST[min_index] = true;

        for (int v = 0; v < num_quadrados; v++)
        {
            if (!inMST[v])
            {
                int px1, py1, px2, py2;
                encontrarPontosConexao(quadrados[min_index], quadrados[v], &px1, &py1, &px2, &py2);
                int distancia = abs(px1 - px2) + abs(py1 - py2);

                if (distancia < key[v])
                {
                    parent[v] = min_index;
                    key[v] = distancia;
                }
            }
        }
    }

    for (int i = 1; i < num_quadrados; i++)
    {
        int px1, py1, px2, py2;
        encontrarPontosConexao(quadrados[parent[i]], quadrados[i], &px1, &py1, &px2, &py2);
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

int encontraCaminhoMapa(Mapa *mapa, int x_inicio, int y_inicio, int x_dest, int y_dest, int solucaoX[], int solucaoY[], int *tamanho_solucao)
{

    int altura = mapa->linhas, largura = mapa->colunas;

    // Se tentou sair do labirinto, este não é o caminho certo.
    if (x_inicio < 0 || x_inicio >= largura || y_inicio < 0 || y_inicio >= altura || x_dest < 0 || x_dest >= largura || y_dest < 0 || y_dest >= altura)
        return 1; // fora do mapa

    // Cria cópia da matriz
    char **matriz_copia = malloc(mapa->linhas * sizeof(char *));
    for (int i = 0; i < mapa->linhas; i++)
    {
        matriz_copia[i] = malloc(mapa->colunas * sizeof(char));
        memcpy(matriz_copia[i], mapa->matriz[i], mapa->colunas);
    }

    // Insere os dados do no mapa_copia
    Mapa *mapa_copia = malloc(sizeof(Mapa));
    mapa_copia->matriz = matriz_copia;
    mapa_copia->linhas = altura;
    mapa_copia->colunas = largura;
    mapa_copia->largura_rua = mapa->largura_rua;

    mapa_copia->matriz[y_inicio][x_inicio] = ORIGEM;
    mapa_copia->matriz[y_dest][x_dest] = DESTINO;

    // Encontrando o caminho
    int resultado = encontraCaminho(x_inicio, y_inicio, mapa_copia->matriz, largura, altura, solucaoX, solucaoY, tamanho_solucao);

    desalocarMapa(mapa_copia);

    return resultado;
}

void gerarMapa(Mapa *mapa, int num_quadrados, int largura_rua, int largura_borda,
               int min_tamanho, int max_tamanho, int distancia_minima)
{
    mapa->largura_rua = largura_rua;
    srand(time(NULL));

    // Verifica se os parâmetros são viáveis
    if (min_tamanho <= 0 || max_tamanho < min_tamanho ||
        mapa->linhas <= 0 || mapa->colunas <= 0)
    {
        return;
    }

    // Ajusta o tamanho máximo se necessário
    int tamanho_max_possivel = MIN(mapa->linhas, mapa->colunas) - 2 * largura_borda;
    if (max_tamanho > tamanho_max_possivel)
    {
        max_tamanho = tamanho_max_possivel;
        if (min_tamanho > max_tamanho)
        {
            min_tamanho = max_tamanho;
        }
    }

    Quadrado quadrados[num_quadrados];
    int count = 0;
    int tentativas = 0;

    while (count < num_quadrados && tentativas < MAX_TENTATIVAS * num_quadrados)
    {
        // Gera um quadrado com tamanho aleatório dentro dos limites
        int tamanho = rand() % (max_tamanho - min_tamanho + 1) + min_tamanho;

        // Verifica se o tamanho é viável no mapa
        if (tamanho + 2 * largura_borda > mapa->linhas ||
            tamanho + 2 * largura_borda > mapa->colunas)
        {
            tentativas++;
            continue;
        }

        // Calcula posições máximas considerando bordas
        int max_x = mapa->linhas - tamanho - largura_borda;
        int max_y = mapa->colunas - tamanho - largura_borda;

        // Se não houver espaço suficiente, reduz a distância mínima
        int dist_atual = distancia_minima;
        if (max_x <= 0 || max_y <= 0)
        {
            dist_atual = 0; // Desativa a distância mínima se não houver espaço
            max_x = mapa->linhas - tamanho;
            max_y = mapa->colunas - tamanho;
            if (max_x <= 0 || max_y <= 0)
                break; // Sai se não couber nenhum quadrado
        }

        Quadrado q = {
            .tamanho = tamanho,
            .x = rand() % (max_x + 1),
            .y = rand() % (max_y + 1)};

        // Verifica colisões com outros quadrados
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
            quadrados[count] = q;
            desenharQuadrado(mapa, q, largura_borda);
            count++;
            tentativas = 0; // Reseta o contador após sucesso
        }
        else
        {
            tentativas++;
        }
    }

    // Conecta os quadrados gerados (mesmo que não tenha atingido o num_quadrados desejado)
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