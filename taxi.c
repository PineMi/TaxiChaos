/*
  pthread_create: Inicia uma nova thread, recebendo como parâmetros um identificador de
thread, atributos (geralmente NULL para atributos padrão), a função que a thread executará e
um ponteiro para os argumentos dessa função

pthread_join: Permite que uma thread espere a finalização de outra, assegurando que a
execução seja sincronizada de maneira básica, já que o código principal ou outra thread só
continuará após o término da thread “juntada”.
*/ 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#define RUA 0
#define CALCADA 1
#define TAMANHO_RUA 4

#define ROWS 5
#define COLS 5



int** criar_matriz_terminal(int* linhas, int* colunas) {
  struct winsize ws;

  // Obtém o tamanho do terminal
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
      perror("Erro ao obter o tamanho do terminal");
      return NULL;
  }

  *linhas = ws.ws_row;   // Passa as dimensões por referência
  *colunas = ws.ws_col;

  // Aloca a matriz dinamicamente
  int** matriz = malloc((*linhas) * sizeof(int*));
  if (matriz == NULL) {
      perror("Erro ao alocar memória para as linhas da matriz");
      return NULL;
  }

  for (int i = 0; i < *linhas; i++) {
      matriz[i] = malloc((*colunas) * sizeof(int));
      if (matriz[i] == NULL) {
          perror("Erro ao alocar memória para as colunas da matriz");
          // Libera linhas já alocadas
          for (int j = 0; j < i; j++) {
              free(matriz[j]);
          }
          free(matriz);
          return NULL;
      }
  }

  // Inicializa a matriz com zeros
  for (int i = 0; i < *linhas; i++) {
      for (int j = 0; j < *colunas; j++) {
          matriz[i][j] = RUA;
      }
  }

  return matriz;
}


// Adiciona blocos retangulares de calçadas apontando para o centro
void adicionar_blocos_calcadas(int **matriz, int linhas, int colunas) {
  int centro_x = linhas / 2;
  int centro_y = colunas / 2;

  srand(time(NULL));

  // Número de blocos de calçadas a serem criados
  int num_blocos = rand() % 10 + 5; // Entre 5 e 15 blocos

  for (int b = 0; b < num_blocos; b++) {
      // Dimensões e posição inicial do bloco
      int altura = rand() % 4 + 2; // Altura entre 2 e 5
      int largura = rand() % 4 + 2; // Largura entre 2 e 5
      int start_x = rand() % linhas;
      int start_y = rand() % colunas;

      // Verifica se o bloco não quebra a conectividade das ruas
      while (start_x != centro_x || start_y != centro_y) {
          // Cria o bloco na posição atual
          for (int i = 0; i < altura && start_x + i < linhas; i++) {
              for (int j = 0; j < largura && start_y + j < colunas; j++) {
                  if (matriz[start_x + i][start_y + j] == RUA) {
                      matriz[start_x + i][start_y + j] = CALCADA; // Marca como calçada
                  }
              }
          }

          // Move o ponto inicial do bloco em direção ao centro
          if (start_x < centro_x) start_x++;
          else if (start_x > centro_x) start_x--;

          if (start_y < centro_y) start_y++;
          else if (start_y > centro_y) start_y--;
      }
  }
}

// Garante que todas as ruas estejam conectadas após os blocos de calçadas serem inseridos
void conectar_ruas(int **matriz, int linhas, int colunas) {
  // Usa BFS para garantir conectividade das ruas
  int **visitado = malloc(linhas * sizeof(int *));
  for (int i = 0; i < linhas; i++) {
      visitado[i] = malloc(colunas * sizeof(int));
      for (int j = 0; j < colunas; j++) {
          visitado[i][j] = 0;
      }
  }

  int dx[] = {-1, 1, 0, 0};
  int dy[] = {0, 0, -1, 1};

  // Inicia BFS no canto superior esquerdo
  int frente = 0, tras = 0;
  int fila_x[linhas * colunas], fila_y[linhas * colunas];
  fila_x[tras] = 0;
  fila_y[tras++] = 0;
  visitado[0][0] = 1;

  while (frente < tras) {
      int x = fila_x[frente];
      int y = fila_y[frente++];
      for (int i = 0; i < 4; i++) {
          int nx = x + dx[i];
          int ny = y + dy[i];
          if (nx >= 0 && ny >= 0 && nx < linhas && ny < colunas &&
              matriz[nx][ny] == RUA && !visitado[nx][ny]) {
              visitado[nx][ny] = 1;
              fila_x[tras] = nx;
              fila_y[tras++] = ny;
          }
      }
  }

  // Conecta ruas que não foram visitadas
  for (int i = 0; i < linhas; i++) {
      for (int j = 0; j < colunas; j++) {
          if (matriz[i][j] == RUA && !visitado[i][j]) {
              matriz[i][j] = CALCADA; // Converte ruas desconectadas para calçadas
          }
      }
  }

  // Libera memória
  for (int i = 0; i < linhas; i++) {
      free(visitado[i]);
  }
  free(visitado);
}




// Imprime a matriz
void imprimir_matriz(int **matriz, int linhas, int colunas) {
  for (int i = 0; i < linhas; i++) {
      for (int j = 0; j < colunas; j++) {
          printf("%d", matriz[i][j]);
      }
      printf("\n");
  }
}


/*
void visualizacao(void* arg){
  struct winsize tamAtualBash;
  int linhas, colunas;
  int** matriz = criar_matriz_terminal(&linhas, &colunas);
  
  
  
  
  
  while(1){
    
  while(quere[inicial]==quere[final]){
      //VERIFICA SE A CONDICAÇÃO AINDA NÃO FOI ATINGIDA
    }
    
    
    
    
    
    
    
    
    
    
    //verifica se o terminal mudou de tamanho
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &tamAtualBash);
    if(tamAtualBash.ws_col != colunas || tamAtualBash.ws_row != linhas){
      int** matriz = criar_matriz_terminal(&linhas, &colunas);
    }
  }
  
  
  
}
*/


//Thread TAXI
void Taxi() {
    return;
}

//Thread PASSAGEIRO
void Passageiro() {
    return;
}



int main() {
    
  struct winsize tamAtualBash;
  int linhas, colunas;
  int** matriz = criar_matriz_terminal(&linhas, &colunas);


    adicionar_blocos_calcadas(matriz, linhas, colunas);
    conectar_ruas(matriz, linhas, colunas);


  imprimir_matriz(matriz, linhas, colunas);

    return 0;
}

//BSF
//https://www.geeksforgeeks.org/breadth-first-search-or-bfs-for-a-graph/ 
