
 .----------------.  .----------------.  .----------------.  .----------------.    .----------------.  .----------------.  .----------------.  .----------------.  .----------------. 
| .--------------. || .--------------. || .--------------. || .--------------. |  | .--------------. || .--------------. || .--------------. || .--------------. || .--------------. |
| |  _________   | || |      __      | || |  ____  ____  | || |     _____    | |  | |     ______   | || |  ____  ____  | || |      __      | || |     ____     | || |    _______   | |
| | |  _   _  |  | || |     /  \     | || | |_  _||_  _| | || |    |_   _|   | |  | |   .' ___  |  | || | |_   ||   _| | || |     /  \     | || |   .'    `.   | || |   /  ___  |  | |
| | |_/ | | \_|  | || |    / /\ \    | || |   \ \  / /   | || |      | |     | |  | |  / .'   \_|  | || |   | |__| |   | || |    / /\ \    | || |  /  .--.  \  | || |  |  (__ \_|  | |
| |     | |      | || |   / ____ \   | || |    > `' <    | || |      | |     | |  | |  | |         | || |   |  __  |   | || |   / ____ \   | || |  | |    | |  | || |   '.___`-.   | |
| |    _| |_     | || | _/ /    \ \_ | || |  _/ /'`\ \_  | || |     _| |_    | |  | |  \ `.___.'\  | || |  _| |  | |_  | || | _/ /    \ \_ | || |  \  `--'  /  | || |  |`\____) |  | |
| |   |_____|    | || ||____|  |____|| || | |____||____| | || |    |_____|   | |  | |   `._____.'  | || | |____||____| | || ||____|  |____|| || |   `.____.'   | || |  |_______.'  | |
| |              | || |              | || |              | || |              | |  | |              | || |              | || |              | || |              | || |              | |
| '--------------' || '--------------' || '--------------' || '--------------' |  | '--------------' || '--------------' || '--------------' || '--------------' || '--------------' |
 '----------------'  '----------------'  '----------------'  '----------------'    '----------------'  '----------------'  '----------------'  '----------------'  '----------------' 
Desenvolvido por: 
Miguel Coratolo Simões Piñeiro
Gabriel Erick Mendes
Bruno Germanetti Ramalho
Camila Nunes Carniel
Mateus Teles Magalhães

Inspirado por: GTA I e Hotline Miami

Bem-vindo ao simulador de táxis mais legal já construído em C! Esse projeto simula um sistema dinâmico de táxis com:

-Múltiplos táxis autônomos

-Passageiros com destinos

-Algoritmos de busca de caminho

-Visualização em tempo real no terminal

Controles interativos

🌟 Funcionalidades
✅ Geração Dinâmica de Mapas - Cidade gerada proceduralmente com ruas e prédios
✅ Simulação Multi-thread - Cada táxi roda em sua própria thread
✅ Busca em Largura (BFS) - Táxis navegam usando BFS
✅ Controles Interativos - Adicione/remova táxis, passageiros, pause/retome
✅ Visualização no Terminal - Interface colorida com emojis

🕹️ Controles
Tecla	Ação
↑	Adiciona um táxi novo
↓	Remove um táxi
P	Adiciona um passageiro
R	Reinicia a simulação
Espaço	Pausa/Continua
S	Mostra status dos táxis
L	Mostra mapa lógico
Q	Sai do programa

🚀 Como Executar
bash
Copy
gcc taxi_simulator.c -o taxi_simulator -lpthread -lncurses
./taxi_simulator

📊 Detalhes Técnicos
Threads: Usa pthread para operações concorrentes dos táxis

Pathfinding: Algoritmo BFS para planejamento de rotas

Entrada/Saída: Input não-bloqueante com termios

Visualização: Renderização com emojis