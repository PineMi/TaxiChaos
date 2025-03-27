#ifndef MAPA_H
#define MAPA_H

#include <stdbool.h>

// Estrutura opaca que representa o mapa
typedef struct Mapa {
    int** matriz;
    int linhas;
    int colunas;
    int largura_rua;
} Mapa;

// Cria um novo mapa do tamanho do terminal
Mapa* criarMapa();

// Gera o mapa com quarteirões e ruas
void gerarMapa(Mapa* mapa, int num_quadrados, int largura_rua, int largura_borda, int min_tamanho, int max_tamanho, int distancia_minima);

// Imprime o mapa no terminal
void imprimirMapa(Mapa* mapa);

// Libera a memória do mapa
void desalocarMapa(Mapa* mapa);

#endif