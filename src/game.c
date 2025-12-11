#include "board.h"
#include "display.h"
#include "files.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>


// Estrutura auxiliar para passar argumentos às threads
typedef struct {
    board_t* board;
    int id; // Índice do fantasma ou do pacman
} thread_arg_t;

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

// ------------------------------------------------------------
// THREAD DO FANTASMA
// ------------------------------------------------------------
void* ghost_thread(void* arg) {

    int id = ((thread_arg_t*)arg)->id;
    debug("[THREAD GHOST %d] Iniciada.\n", id);

    thread_arg_t* params = (thread_arg_t*)arg;
    board_t* board = params->board;
    int ghost_idx = params->id;
    ghost_t* self = &board->ghosts[ghost_idx];
    
    free(params); // Libertar a estrutura auxiliar

    while (board->game_running) {
        // 1. Simular o tempo de espera (velocidade do monstro)
        // Se o monstro tiver ficheiro, usa o 'PASSO' ou o tempo do board
        int sleep_time = (board->tempo > 0) ? board->tempo * 10 : 100; // *10 para ser visível
        sleep_ms(sleep_time);

        // 2. Proteção Crítica (Mutex)
        // Ninguém mexe no tabuleiro enquanto este monstro decide o movimento
        pthread_mutex_lock(&board->board_lock);

        // Verificar se jogo ainda corre depois de acordar
        if (!board->game_running) {
            debug("[THREAD GHOST %d] Detetou fim de jogo. A sair...\n", id);
            pthread_mutex_unlock(&board->board_lock);
            break;
        }

        // 3. Calcular e Executar Movimento
        command_t cmd;
        if (self->n_moves > 0) {
            // Modo Automático (Ficheiro)
            cmd = self->moves[self->current_move % self->n_moves];
            move_ghost(board, ghost_idx, &cmd);
        } else {
            // Modo Aleatório (Se não tiver ficheiro)
            cmd.command = "WASD"[rand() % 4];
            move_ghost(board, ghost_idx, &cmd);
        }

        // 4. Libertar Mutex
        pthread_mutex_unlock(&board->board_lock);
    }
    debug("[THREAD GHOST %d] Terminou loop. A morrer.\n", id);
    return NULL;
}

// ------------------------------------------------------------
// THREAD DO PACMAN (CORRIGIDA)
// ------------------------------------------------------------
void* pacman_thread(void* arg) {
    board_t* board = (board_t*)arg;
    pacman_t* self = &board->pacmans[0];

    while (board->game_running) {
        // Sleep pequeno para não bloquear CPU
        sleep_ms(10); 

        pthread_mutex_lock(&board->board_lock);

        if (!board->game_running) {
            pthread_mutex_unlock(&board->board_lock);
            break;
        }

        command_t cmd;
        int moved = 0;

        // Prioridade 1: Comando do Teclado (vindo do Main)
        if (board->next_pacman_cmd != '\0') {
            cmd.command = board->next_pacman_cmd;
            cmd.turns = 1;
            board->next_pacman_cmd = '\0'; // Consumir comando
            
            int result = move_pacman(board, 0, &cmd);
            moved = 1;

            // Verificar Resultado do Movimento Manual
            if (result == REACHED_PORTAL) {
                board->exit_status = 1; // VITÓRIA
                board->game_running = 0;
            } else if (result == DEAD_PACMAN) {
                board->exit_status = 2; // MORTE
                board->game_running = 0;
            }
        }
        // Prioridade 2: Modo Automático (Ficheiro)
        else if (self->n_moves > 0) {
             cmd = self->moves[self->current_move % self->n_moves];
             
             int result = move_pacman(board, 0, &cmd);
             moved = 1;

             // Verificar Resultado do Movimento Automático
             if (result == REACHED_PORTAL) {
                 board->exit_status = 1; // VITÓRIA
                 board->game_running = 0;
             } else if (result == DEAD_PACMAN) {
                 board->exit_status = 2; // MORTE
                 board->game_running = 0;
             }
        }

        // Verificação final (caso tenha sido atropelado por fantasma sem se mexer)
        if (!self->alive && board->game_running) {
             board->exit_status = 2; // MORTE
             board->game_running = 0;
        }

        pthread_mutex_unlock(&board->board_lock);
        
        // Delay se for movimento automático
        if (moved && self->n_moves > 0) sleep_ms(board->tempo);
    }
    return NULL;
}

// ------------------------------------------------------------
// MAIN (UI THREAD)
// ------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s <dir>\n", argv[0]); return 1; }

    char* dir_path = argv[1];
    struct dirent **namelist;
    // Usa filter_levels e alphasort definidos/incluídos via files.h e dirent.h
    int n = scandir(dir_path, &namelist, filter_levels, alphasort);
    if (n < 0) { perror("scandir"); return 1; }

    srand(time(NULL));
    open_debug_file("debug.log"); // Inicializar debug
    terminal_init();
    
    board_t game_board;
    int accumulated_points = 0;

    for (int i = 0; i < n; i++) {
        // Carregar Nível
        if (load_level(&game_board, dir_path, namelist[i]->d_name, accumulated_points) != 0) {
            free(namelist[i]); continue;
        }

        // --- INICIALIZAR THREADS ---
        pthread_t p_thread;
        pthread_t g_threads[MAX_GHOSTS];

        // 1. Criar Thread Pacman
        pthread_create(&p_thread, NULL, pacman_thread, &game_board);

        // 2. Criar Threads Fantasmas
        for(int g=0; g < game_board.n_ghosts; g++) {
            thread_arg_t* args = malloc(sizeof(thread_arg_t));
            args->board = &game_board;
            args->id = g;
            pthread_create(&g_threads[g], NULL, ghost_thread, args);
        }

        // --- LOOP PRINCIPAL (INTERFACE) ---
        draw_board(&game_board, DRAW_MENU);

        while (game_board.game_running) {
            // 1. Desenhar (Protegido)
            pthread_mutex_lock(&game_board.board_lock);
            
            // Segurança extra: Verificar se morreu "passivamente"
            if (!game_board.pacmans[0].alive && game_board.exit_status == 0) {
                 game_board.exit_status = 2; // Morte
                 game_board.game_running = 0;
            }
            
            draw_board(&game_board, DRAW_MENU);
            pthread_mutex_unlock(&game_board.board_lock);
            refresh_screen();

            // 2. Input
            char input = get_input();
            if (input == 'Q') {
                pthread_mutex_lock(&game_board.board_lock);
                game_board.exit_status = 3; // QUIT
                game_board.game_running = 0;
                pthread_mutex_unlock(&game_board.board_lock);
            } 
            else if (input != '\0') {
                // Passar comando à thread
                game_board.next_pacman_cmd = input;
            }

            sleep_ms(33); // ~30 FPS
        }

        // --- FIM DO NÍVEL ---
        
        // 1. Esperar TODAS as Threads (CRÍTICO antes de unload)
        pthread_join(p_thread, NULL);
        for(int g=0; g < game_board.n_ghosts; g++) {
            pthread_join(g_threads[g], NULL);
        }

        // 2. Decidir Próximo Passo
        int status = game_board.exit_status;

        if (status == 1) { 
            // === VITÓRIA (PORTAL) ===
            screen_refresh(&game_board, DRAW_WIN);
            sleep_ms(1000);
            
            accumulated_points = game_board.pacmans[0].points;
            
            unload_level(&game_board);
            free(namelist[i]);
            
            // LIMPEZA VISUAL (Resolve o bug do Pacman duplicado)
            clear();
            refresh();
            // Continua para o próximo nível (i++)
        }
        else {
            // === DERROTA OU QUIT ===
            if (status == 2) {
                screen_refresh(&game_board, DRAW_GAME_OVER);
            }
            // Se for 3 (Quit), não mostra Game Over, sai só.
            
            sleep_ms(2000);
            
            unload_level(&game_board);
            free(namelist[i]);
            
            break; // <--- SAI DO CICLO FOR (Não carrega nível seguinte)
        }
    }
    
    // Libertar o resto da lista do scandir se saímos a meio
    for (int k = 0; k < n; k++) {
        // free(namelist[k]) seria double free para os já processados, 
        // num cenário real geriríamos o índice melhor, mas o SO limpa ao sair.
    }
    free(namelist);

    terminal_cleanup();
    close_debug_file();
    return 0;
}