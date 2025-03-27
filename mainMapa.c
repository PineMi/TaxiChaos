#include "mapa.h"
#include <stdio.h>

int main() {
    // Cria o mapa
    Mapa* mapa = criarMapa();
    if (!mapa) return 1;
    
    // Gera o mapa com 6 quarteirões e ruas de largura 3
    gerarMapa(mapa, 15, 2, 2, 10, 12, 8);
    
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
