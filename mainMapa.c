#include "mapa.h"

int main() {
    // Cria o mapa
    Mapa* mapa = criarMapa();
    if (!mapa) return 1;
    
    // Gera o mapa com 6 quarteirões e ruas de largura 3
    gerarMapa(mapa, 15, 2, 2, 10, 12, 8);
    
    // Imprime o mapa
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
