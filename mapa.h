#ifndef MAPA_H
#define MAPA_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/ioctl.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_TENTATIVAS 1000

#define MAP_VERTICAL_PROPORTION 0.5
#define MAP_HORIZONTAL_PROPORTION 0.4

#define RUA 0
#define CALCADA 1

#define CALCADA_EMOJI "⬛"  
#define RUA_EMOJI "⬜"       
#define PASSENGER_EMOJI "🙋" 
#define DESTINO_EMOJI "🏠"   
#define TAXI_EMOJI "🚖"
#define PASSENGER_PONTO_EMOJI "🔲"

#define R_TAXI_LIVRE 100 //100 - 199 taxis livres
#define R_TAXI_OCUPADO 200 //200 - 299 taxis livres
#define R_PASSENGER 300 //300 - 399 taxis livres
#define R_DESTINOS_PASSAGEIROS 400 // 400 - 499 taxis livres
#define R_PONTO_PASSAGEIRO 500 // 500 - 599 taxis livres

// Quadrado (block) structure
typedef struct {
    int x, y;
    int tamanho;
} Quadrado;


// Mapa structure
typedef struct {
    int linhas, colunas;
    int largura_rua;
    int **matriz;
    pthread_mutex_t lock; 
} Mapa;

// Node for BFS
typedef struct {
    int x;
    int y;
    int parent_index;
} Node;

 Mapa* criarMapa();

 int encontraCaminhoCoordenadas(int col_inicio, int lin_inicio, int col_destino, int lin_destino,
    int **maze, int num_colunas, int num_linhas,
    int solucaoCol[], int solucaoLin[], int *tamanho_solucao);

 void imprimirMapaLogico(Mapa* mapa);

 bool find_random_free_point(Mapa* mapa, int* random_x, int* random_y);

 bool find_random_free_point_adjacent_to_calcada(Mapa* mapa, int* free_x, int* free_y, int* calcada_x, int* calcada_y);

 void gerarMapa(Mapa* mapa, int num_quadrados, int largura_rua, int largura_borda, int min_tamanho, int max_tamanho, int distancia_minima);

void desalocarMapa(Mapa* mapa);

int encontraCaminho(int col_inicio, int lin_inicio, int **maze, int num_colunas, int num_linhas,
    int solucaoCol[], int solucaoLin[], int *tamanho_solucao, int destino);

 void renderMap(Mapa* mapa);

#endif