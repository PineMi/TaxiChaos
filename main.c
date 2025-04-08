#include "threadsTaxi.h"

// Comandos
// p - Criar passageiro
// r - Resetar mapa
// l - Imprimir mapa lógico
// s - Status taxis
// q - Sair
// ↑ - Criar taxi
// ↓ - Destruir taxi
// Space - Pausar/Retomar (Obs, visualizador continua funcionando)

// Entidades na matriz
// 0 - Caminho livre
// 1 - Calçada/Prédios
// 100 - 199 Taxis Livres
// 200 - 299 Taxis Ocupados
// 300 - 399 Passageiros
// 400 - 499 Destinos passageiros
// 500 - 599 Rotas de Taxis (Marcação para não gerar um passageiro em uma rota de um taxi)

// - Jogar os comandos de letra para lowercase no input_thread

// Lista de Demandas
// - SPAWN COM IDs
// - Render por IDs
// - No Spawn de passageiro BFS para o taxi livre mais próximo
// - Rota do taxi selecionado até o passageiro
// - Chegada no passageiro (Atualizar a matriz) 
// - Rota até Destino
// - Chegada

// -------------------- QUEUE FUNCTIONS --------------------



int main() {
    init_operations();
    return 0;
}