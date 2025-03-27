#include "mapa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>

#define RUA 0
#define CALCADA 1

typedef struct {
    int x, y;
    int tamanho;
} Quadrado;

struct Mapa {
    int** matriz;
    int linhas;
    int colunas;
    int largura_rua;
};

int encontraCaminhoMatriz(char **mapa, int largura, int altura, int x_inicio, int y_inicio ,int x_dest, int y_dest, int solucaoX[], int solucaoY[], int *tamanho_solucao) {
    // Se tentou sair do labirinto, este não é o caminho certo.
    if (x_inicio < 0 || x_inicio >= largura || y_inicio < 0 || y_inicio >= altura) return 1;

    // Alocando memória para as matrizes
    int** mapa_copia = malloc(altura * sizeof(int*));

    for (int i = 0; i < altura; i++) {
        mapa_copia[i] = malloc(largura * sizeof(int));
    }

    // Copiando os dados da matriz original para a matriz_copia
    for (int i = 0; i < altura; i++) {
        memcpy(mapa_copia[i], mapa[i], largura * sizeof(int));
    }

    mapa_copia[x_inicio][y_inicio] = 'E';
    mapa_copia[x_dest][y_dest] = 'S';
    
    // Encontrando o caminho
    if(encontraCaminho(x_inicio, y_inicio, mapa_copia, largura, altura, solucaoX, solucaoY, tamanho_solucao) == 0) {

        // Se não encontrou caminho, libera a memória e retorna 0
        desalocarMapa(mapa_copia);
        return 1;
    }

    desalocarMapa(mapa_copia);
    return 0;

}



int encontraCaminho(int x_atual, int y_atual, char **maze, int largura, int altura, int solucaoX[], int solucaoY[], int *tamanho_solucao) {
    // Se tentou sair do labirinto, este não é o caminho certo.
    if (x_atual < 0 || x_atual >= largura || y_atual < 0 || y_atual >= altura) return 0;

    char aqui = maze[x_atual][y_atual];

    // Verifica se achou a saída.

    if (aqui == 'S') return 1;

    // Se bateu na parede ou voltou para algum lugar que já esteve,
    // então este não é o caminho certo.
    if (aqui == 'X' || aqui == '>' || aqui == '<' || aqui == 'v' || aqui == '^') return 0;

    // Tenta ir para cima.
    maze[x_atual][y_atual] = '^';
    solucaoX[*tamanho_solucao] = x_atual;
    solucaoY[*tamanho_solucao] = y_atual;
    (*tamanho_solucao)++;
    if (encontraCaminho(x_atual, y_atual + 1, maze, largura, altura, solucaoX, solucaoY, tamanho_solucao)) return 1;

    // Tenta ir para baixo.
    maze[x_atual][y_atual] = 'v';
    solucaoX[*tamanho_solucao] = x_atual;
    solucaoY[*tamanho_solucao] = y_atual;
    (*tamanho_solucao)++;
    if (encontraCaminho(x_atual, y_atual - 1, maze, largura, altura, solucaoX, solucaoY, tamanho_solucao)) return 1;

    // Tenta ir para a esquerda.
    maze[x_atual][y_atual] = '<';
    solucaoX[*tamanho_solucao] = x_atual;
    solucaoY[*tamanho_solucao] = y_atual;
    (*tamanho_solucao)++;
    if (encontraCaminho(x_atual - 1, y_atual, maze, largura, altura, solucaoX, solucaoY, tamanho_solucao)) return 1;

    // Tenta ir para a direita.
    maze[x_atual][y_atual] = '>';
    solucaoX[*tamanho_solucao] = x_atual;
    solucaoY[*tamanho_solucao] = y_atual;
    (*tamanho_solucao)++;
    if (encontraCaminho(x_atual + 1, y_atual, maze, largura, altura, solucaoX, solucaoY, tamanho_solucao)) return 1;

    // Não deu, então volta.
    solucaoX[*tamanho_solucao] = '0';
    solucaoY[*tamanho_solucao] = '0';
    (*tamanho_solucao)--;
    maze[x_atual][y_atual] = 'O';   
    return 0;
}

// Funções auxiliares estáticas (visíveis apenas neste arquivo)
static void desenharQuadrado(Mapa* mapa, Quadrado q, int largura_borda) {
    // Topo
    for (int i = 0; i < largura_borda; i++) {
        for (int j = q.y; j < q.y + q.tamanho && j < mapa->colunas; j++) {
            if (q.x + i < mapa->linhas) mapa->matriz[q.x + i][j] = RUA;
        }
    }
    
    // Base
    for (int i = 0; i < largura_borda; i++) {
        for (int j = q.y; j < q.y + q.tamanho && j < mapa->colunas; j++) {
            if (q.x + q.tamanho - i - 1 < mapa->linhas) mapa->matriz[q.x + q.tamanho - i - 1][j] = RUA;
        }
    }
    
    // Laterais
    for (int i = 0; i < largura_borda; i++) {
        for (int j = q.x; j < q.x + q.tamanho && j < mapa->linhas; j++) {
            if (q.y + i < mapa->colunas) mapa->matriz[j][q.y + i] = RUA;
            if (q.y + q.tamanho - i - 1 < mapa->colunas) mapa->matriz[j][q.y + q.tamanho - i - 1] = RUA;
        }
    }
}

static void encontrarPontosConexao(Quadrado a, Quadrado b, int* px1, int* py1, int* px2, int* py2) {
    int pontos_a[4][2] = {
        {a.x + a.tamanho/2, a.y},                     // Topo
        {a.x + a.tamanho/2, a.y + a.tamanho - 1},      // Base
        {a.x, a.y + a.tamanho/2},                      // Esquerda
        {a.x + a.tamanho - 1, a.y + a.tamanho/2}       // Direita
    };

    int pontos_b[4][2] = {
        {b.x + b.tamanho/2, b.y},
        {b.x + b.tamanho/2, b.y + b.tamanho - 1},
        {b.x, b.y + b.tamanho/2},
        {b.x + b.tamanho - 1, b.y + b.tamanho/2}
    };

    int min_dist = INT_MAX;
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int dx = pontos_a[i][0] - pontos_b[j][0];
            int dy = pontos_a[i][1] - pontos_b[j][1];
            int dist = dx * dx + dy * dy;
            
            if (dist < min_dist) {
                min_dist = dist;
                *px1 = pontos_a[i][0];
                *py1 = pontos_a[i][1];
                *px2 = pontos_b[j][0];
                *py2 = pontos_b[j][1];
            }
        }
    }
}

static void desenharRua(Mapa* mapa, int x1, int y1, int x2, int y2) {
    // Desenha segmento horizontal
    if (y1 != y2) {
        int passo_y = (y2 > y1) ? 1 : -1;
        for (int y = y1; y != y2; y += passo_y) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int x = x1 + k - mapa->largura_rua/2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas) {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }
    
    // Desenha segmento vertical
    if (x1 != x2) {
        int passo_x = (x2 > x1) ? 1 : -1;
        for (int x = x1; x != x2; x += passo_x) {
            for (int k = 0; k < mapa->largura_rua; k++) {
                int y = y2 + k - mapa->largura_rua/2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas) {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }
    
    // Preenche o canto entre os segmentos
    if (x1 != x2 && y1 != y2) {
        for (int kx = 0; kx < mapa->largura_rua; kx++) {
            for (int ky = 0; ky < mapa->largura_rua; ky++) {
                int x = x1 + kx - mapa->largura_rua/2;
                int y = y2 + ky - mapa->largura_rua/2;
                if (x >= 0 && x < mapa->linhas && y >= 0 && y < mapa->colunas) {
                    mapa->matriz[x][y] = RUA;
                }
            }
        }
    }
}

static void conectarQuadradosMST(Mapa* mapa, Quadrado* quadrados, int num_quadrados) {
    int* parent = malloc(num_quadrados * sizeof(int));
    int* key = malloc(num_quadrados * sizeof(int));
    bool* inMST = malloc(num_quadrados * sizeof(bool));

    for (int i = 0; i < num_quadrados; i++) {
        key[i] = INT_MAX;
        inMST[i] = false;
    }

    key[0] = 0;
    parent[0] = -1;

    for (int count = 0; count < num_quadrados - 1; count++) {
        int min_key = INT_MAX, min_index;
        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v] && key[v] < min_key) {
                min_key = key[v];
                min_index = v;
            }
        }

        inMST[min_index] = true;

        for (int v = 0; v < num_quadrados; v++) {
            if (!inMST[v]) {
                int px1, py1, px2, py2;
                encontrarPontosConexao(quadrados[min_index], quadrados[v], &px1, &py1, &px2, &py2);
                int distancia = abs(px1 - px2) + abs(py1 - py2);
                
                if (distancia < key[v]) {
                    parent[v] = min_index;
                    key[v] = distancia;
                }
            }
        }
    }

    for (int i = 1; i < num_quadrados; i++) {
        int px1, py1, px2, py2;
        encontrarPontosConexao(quadrados[parent[i]], quadrados[i], &px1, &py1, &px2, &py2);
        desenharRua(mapa, px1, py1, px2, py2);
    }

    free(parent);
    free(key);
    free(inMST);
}

// Implementações das funções públicas
Mapa* criarMapa() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("Erro ao obter tamanho do terminal");
        return NULL;
    }

    Mapa* mapa = malloc(sizeof(Mapa));
    mapa->linhas = ws.ws_row;
    mapa->colunas = ws.ws_col;
    mapa->largura_rua = 1; // Valor padrão

    mapa->matriz = malloc(mapa->linhas * sizeof(int*));
    for (int i = 0; i < mapa->linhas; i++) {
        mapa->matriz[i] = malloc(mapa->colunas * sizeof(int));
        for (int j = 0; j < mapa->colunas; j++) {
            mapa->matriz[i][j] = CALCADA;
        }
    }

    return mapa;
}

void gerarMapa(Mapa* mapa, int num_quadrados, int largura_rua, int largura_borda, int min_tamanho, int max_tamanho, int distancia_minima) {
    mapa->largura_rua = largura_rua;

    Quadrado quadrados[num_quadrados];
    int count = 0;
    srand(time(NULL));

    while (count < num_quadrados) {
        Quadrado q = {
            .tamanho = rand() % (max_tamanho - min_tamanho + 1) + min_tamanho,
            .x = rand() % (mapa->linhas - q.tamanho - largura_borda),
            .y = rand() % (mapa->colunas - q.tamanho - largura_borda)
        };

        bool valido = true;
        for (int i = 0; i < count; i++) {
            int dx = abs((q.x + q.tamanho/2) - (quadrados[i].x + quadrados[i].tamanho/2));
            int dy = abs((q.y + q.tamanho/2) - (quadrados[i].y + quadrados[i].tamanho/2));
            if (dx < distancia_minima && dy < distancia_minima) {
                valido = false;
                break;
            }
        }

        if (valido) {
            quadrados[count] = q;
            desenharQuadrado(mapa, q, largura_borda);
            count++;
        }
    }

    conectarQuadradosMST(mapa, quadrados, num_quadrados);
}

void imprimirMapa(Mapa* mapa) {
    for (int i = 0; i < mapa->linhas; i++) {
        for (int j = 0; j < mapa->colunas; j++) {
            printf("%d", mapa->matriz[i][j]);
        }
        printf("\n");
    }
}

void desalocarMapa(Mapa* mapa) {
    for (int i = 0; i < mapa->linhas; i++) {
        free(mapa->matriz[i]);
    }
    free(mapa->matriz);
    free(mapa);
}