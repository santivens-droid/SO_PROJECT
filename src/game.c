#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h> // Para scandir

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2

// Função de filtro para scandir
int filter_levels(const struct dirent *entry) {
    const char *dot = strrchr(entry->d_name, '.');
    if (dot && strcmp(dot, ".lvl") == 0) {
        return 1;
    }
    return 0;
}

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int play_board(board_t * game_board) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play = NULL; // Inicializar a NULL por segurança
    
    // Variável estática ou local para o comando manual
    command_t manual_command; 

    // 1. Ler Input (Agora é Non-Blocking graças ao display.c)
    char input = get_input();

    // 2. Verificar Saída Global
    if (input == 'Q') {
        return QUIT_GAME;
    }
    
    // 3. Lógica do Pacman
    if (pacman->n_moves == 0) { 
        // --- Modo Manual ---
        if (input != '\0') {
            // Se o jogador carregou numa tecla, preparamos o comando
            manual_command.command = input;
            manual_command.turns = 1;
            play = &manual_command;
            
            // Debug para ver o que carregaste
            debug("KEY %c\n", play->command);
            
            // Executar movimento do Pacman
            int result = move_pacman(game_board, 0, play);
            if (result == REACHED_PORTAL) return NEXT_LEVEL;
            if (result == DEAD_PACMAN) return QUIT_GAME;
        }
        // SE NÃO CARREGOU EM NADA: O Pacman fica parado, 
        // MAS o código continua para baixo para mexer os fantasmas!
    }
    else { 
        // --- Modo Automático ---
        play = &pacman->moves[pacman->current_move % pacman->n_moves];
        
        int result = move_pacman(game_board, 0, play);
        if (result == REACHED_PORTAL) return NEXT_LEVEL;
        if (result == DEAD_PACMAN) return QUIT_GAME;
    }

    // 4. Lógica dos Fantasmas (Corre SEMPRE, independentemente do input)
    for (int i = 0; i < game_board->n_ghosts; i++) {
        ghost_t* ghost = &game_board->ghosts[i];
        
        // Verifica se o monstro tem movimentos definidos
        if (ghost->n_moves > 0) {
             move_ghost(game_board, i, &ghost->moves[ghost->current_move % ghost->n_moves]);
        }
    }

    // 5. Verificação Final de Vida
    if (!game_board->pacmans[0].alive) {
        return QUIT_GAME;
    }      

    return CONTINUE_PLAY;  
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        return 1;
    }

    char* dir_path = argv[1];
    struct dirent **namelist;
    int n;

    // POSIX scandir para encontrar ficheiros .lvl e ordená-los
    n = scandir(dir_path, &namelist, filter_levels, alphasort);
    if (n < 0) {
        perror("scandir");
        return 1;
    }

    srand((unsigned int)time(NULL));
    open_debug_file("debug.log");
    terminal_init();
    
    int accumulated_points = 0;
    board_t game_board;

    // Loop through levels
    for (int i = 0; i < n; i++) {
        char* level_file = namelist[i]->d_name;
        
        // Carregar nível
        if (load_level(&game_board, dir_path, level_file, accumulated_points) != 0) {
            debug("Failed to load level %s\n", level_file);
            free(namelist[i]);
            continue;
        }

        draw_board(&game_board, DRAW_MENU);
        refresh_screen();

        int level_result = CONTINUE_PLAY;
        bool end_game = false;

        while (!end_game) {
            level_result = play_board(&game_board); 

            if(level_result == NEXT_LEVEL) {
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(game_board.tempo * 10); // Pausa breve na vitória
                accumulated_points = game_board.pacmans[0].points;
                end_game = true;
            }
            else if(level_result == QUIT_GAME) {
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo * 20);
                end_game = true;
            }
            else {
                screen_refresh(&game_board, DRAW_MENU); 
            }
        }
        
        unload_level(&game_board);
        free(namelist[i]);

        if (level_result == QUIT_GAME) break;
    }
    free(namelist);

    terminal_cleanup();
    close_debug_file();

    return 0;
}