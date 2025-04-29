TAXI CHAOS

Desenvolvido por: 
Miguel Coratolo Simões Piñeiro
Gabriel Erick Mendes
Bruno Germanetti Ramalho
Camila Nunes Carniel
Mateus Teles Magalhães

Bem-vindo ao nosso simulador de táxis construído em C! Esse projeto simula um sistema dinâmico de táxis com:

-Múltiplos táxis autônomos

-Passageiros com destinos

-Algoritmos de busca de caminho

-Visualização em tempo real no terminal

- Centro de Controle de Taxis

- Controles interativos

🌟 Funcionalidades
✅ Geração Dinâmica de Mapas - Cidade gerada proceduralmente com ruas (MST) e prédios
✅ Simulação Multi-thread - Cada táxi roda em sua própria thread
✅ Busca em Largura (BFS) - Táxis navegam usando BFS
✅ Controles Interativos - Adicione/remova táxis, passageiros, pause/retome
✅ Visualização no Terminal - Interface colorida com emojis

🕹️ Controles
Tecla	Ação
↑       Adiciona um táxi novo
↓       Remove um táxi
P       Adiciona um passageiro
R	    Reinicia a simulação
Espaço  Pausa/Continua
L       Mostra mapa lógico
Q       Sai do programa

🚀 Como Executar

Encorajamos você a brincar com as definições inicias do main.c para testar diferentes configurações.

Em terminal bash:
gcc taxi_simulator.c -o taxi_simulator -lpthread -lncurses
./taxi_simulator

📊 Detalhes Técnicos

Threads: Usa pthread para operações concorrentes dos táxis

Pathfinding: Algoritmo BFS para planejamento de rotas e MST para criação de Ruas

Entrada/Saída: Input não-bloqueante com termios

Visualização: Renderização com emojis

OBS.: Precisa ser inicializado em ambiente LINUX (para uma melhor experiência, execulte o programa em BASH com UTF-8)
