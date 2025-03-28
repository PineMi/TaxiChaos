#include "mapa.h"
#include <stdio.h>

int main() {
    // Cria o mapa
    Mapa* mapa = criarMapa();
    if (!mapa) return 1;
    
    int numero_Quadrados = 12;
    int Largura_rua = 2;
    int Largura_Borda = 2;
    int minimo_tamanho = 16;
    int maximo_tamanho = 18;
    int Distancia_minima = maximo_tamanho;
    



    // Gera o mapa com 6 quarteirões e ruas de largura 3
    gerarMapa(mapa, numero_Quadrados, Largura_rua, Largura_Borda, minimo_tamanho, maximo_tamanho, Distancia_minima);
    
    // Imprime o mapa
    imprimirMapa(mapa);

    int inicioX, inicioY, destinoX, destinoY;
    int solucaoX[10000], solucaoY[10000];
    int tamanho_solucao = 0;

    scanf("%d %d %d %d", &inicioX, &inicioY, &destinoX, &destinoY);

    encontraCaminhoMapa(mapa, inicioX-2, inicioY, destinoX-2, destinoY, solucaoX, solucaoY, &tamanho_solucao);

    marcarCaminho(mapa->matriz, solucaoX, solucaoY, tamanho_solucao);

    imprimirMapa(mapa);

    // Libera a memória
    desalocarMapa(mapa);
    

    /*
    PARA RODAR USE

    gcc -c mapa.c -o mapa.o
    gcc main.c mapa.o -o programa
    ./programa

    */

    return 0;
}
