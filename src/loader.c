#include "loader.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>      // POSIX Open
#include <sys/stat.h>   // POSIX Stat
#include <unistd.h>     // POSIX Read/Close

/* -------------------------------------------------------------------------- */
/* FUNÇÕES AUXILIARES LOCAIS (STATIC)                                         */
/* -------------------------------------------------------------------------- */

// Lê um ficheiro inteiro para um buffer
static char* read_file_to_buffer(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir ficheiro");
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    char* buffer = malloc(st.st_size + 1);
    if (!buffer) {
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd, buffer, st.st_size);
    if (bytes_read != st.st_size) {
        free(buffer);
        close(fd);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    close(fd);
    return buffer;
}

// Avança para a próxima linha
static char* next_line(char* current) {
    char* eol = strchr(current, '\n');
    if (!eol) return NULL;
    return eol + 1;
}

// Faz parse de ficheiros de agentes (.m ou .p)
static int parse_agent_file(const char* filepath, int* start_x, int* start_y, int* passo, command_t* moves, int* n_moves) {
    char* buffer = read_file_to_buffer(filepath);
    if (!buffer) return -1;

    char* line = buffer;
    *n_moves = 0;
    *passo = 0; 

    while (line && *line) {
        if (*line == '#' || *line == '\n' || *line == '\r') {
            line = next_line(line);
            continue;
        }
        
        char key[16];
        if (sscanf(line, "%15s", key) == 1) {
            if (strcmp(key, "PASSO") == 0) {
                sscanf(line, "PASSO %d", passo);
            }
            else if (strcmp(key, "POS") == 0) {
                sscanf(line, "POS %d %d", start_y, start_x);
            }
            else {
                char cmd_char;
                int turns = 1;
                char* ptr = line;
                while (*ptr && isspace(*ptr)) ptr++; 
                
                cmd_char = *ptr;
                ptr++;
                if (*ptr && isdigit(*ptr)) {
                    turns = atoi(ptr);
                }

                if (*n_moves < MAX_MOVES) {
                    moves[*n_moves].command = cmd_char;
                    moves[*n_moves].turns = turns;
                    moves[*n_moves].turns_left = turns;
                    (*n_moves)++;
                }
            }
        }
        line = next_line(line);
    }

    free(buffer);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* FUNÇÕES PÚBLICAS                                                           */
/* -------------------------------------------------------------------------- */

int load_level(board_t* board, const char* dir_path, const char* level_file, int accumulated_points) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, level_file);
    
    char* buffer = read_file_to_buffer(filepath);
    if (!buffer) return -1;

    board->n_pacmans = 0;
    board->n_ghosts = 0;
    board->pacman_file[0] = '\0';
    snprintf(board->level_name, sizeof(board->level_name), "%s", level_file);

    char* line = buffer;
    int reading_map = 0;
    int map_row = 0;

    // 1. Ler Metadados e Mapa
    while (line && *line) {
        if (*line == '#') { line = next_line(line); continue; }
        
        char key[16];
        if (!reading_map && sscanf(line, "%15s", key) == 1) {
            if (strcmp(key, "DIM") == 0) {
                sscanf(line, "DIM %d %d", &board->height, &board->width);
                board->board = calloc(board->width * board->height, sizeof(board_pos_t));
            }
            else if (strcmp(key, "TEMPO") == 0) {
                sscanf(line, "TEMPO %d", &board->tempo);
            }
            else if (strcmp(key, "PAC") == 0) {
                sscanf(line, "PAC %s", board->pacman_file);
                board->n_pacmans = 1;
            }
            else if (strcmp(key, "MON") == 0) {
                char* p = line + 3; 
                while (*p && board->n_ghosts < MAX_GHOSTS) {
                    while (*p && isspace(*p)) p++;
                    if (!*p) break;
                    
                    char mon_file[256];
                    int len = 0;
                    while (*p && !isspace(*p) && len < 255) {
                        mon_file[len++] = *p++;
                    }
                    mon_file[len] = '\0';
                    strcpy(board->ghosts_files[board->n_ghosts], mon_file);
                    board->n_ghosts++;
                }
            }
            else if (strchr("Xo@", *line)) {
                reading_map = 1;
            }
        }
        
        if (reading_map) {
             for (int i = 0; i < board->width && line[i] != '\0' && line[i] != '\n'; i++) {
                 int idx = get_board_index(board->width, i, map_row);
                 char c = line[i];
                 if (c == 'X') board->board[idx].content = 'W';
                 else if (c == '@') {
                     board->board[idx].content = ' ';
                     board->board[idx].has_portal = 1;
                 }
                 else if (c == 'o' || c == '0') {
                     board->board[idx].content = ' ';
                     board->board[idx].has_dot = 1;
                 }
                 else {
                     board->board[idx].content = ' ';
                 }
             }
             map_row++;
        }
        line = next_line(line);
    }
    free(buffer);

    // 2. Alocar Agentes
    board->pacmans = calloc(1, sizeof(pacman_t)); 
    board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));

    // 3. Carregar Pacman
    if (board->n_pacmans > 0) {
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, board->pacman_file);
        parse_agent_file(filepath, &board->pacmans[0].pos_x, &board->pacmans[0].pos_y, 
                         &board->pacmans[0].passo, board->pacmans[0].moves, &board->pacmans[0].n_moves);
        
        pacman_t* p = &board->pacmans[0];
        p->alive = 1;
        p->points = accumulated_points;
        int idx = get_board_index(board->width, p->pos_x, p->pos_y);
        board->board[idx].content = 'P';
        board->board[idx].has_dot = 0; 
    } else {
        // Fallback manual
        board->n_pacmans = 1;
        board->pacmans[0].pos_x = 1;
        board->pacmans[0].pos_y = 1;
        board->pacmans[0].alive = 1;
        board->pacmans[0].points = accumulated_points;
        board->board[get_board_index(board->width, 1, 1)].content = 'P';
    }

    // 4. Carregar Fantasmas
    for (int i = 0; i < board->n_ghosts; i++) {
        board->ghosts[i].pos_x = -1;
        board->ghosts[i].pos_y = -1;

        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, board->ghosts_files[i]);
        
        parse_agent_file(filepath, &board->ghosts[i].pos_x, &board->ghosts[i].pos_y, 
                         &board->ghosts[i].passo, board->ghosts[i].moves, &board->ghosts[i].n_moves);
        
        ghost_t* g = &board->ghosts[i];
        
        if (g->pos_x >= 0 && g->pos_x < board->width && 
            g->pos_y >= 0 && g->pos_y < board->height) {
            
            int idx = get_board_index(board->width, g->pos_x, g->pos_y);
            if (board->board[idx].content != 'W') {
                board->board[idx].content = 'M';
            }
        } 
    }

    return 0;
}

void unload_level(board_t * board) {
    if (board->board) free(board->board);
    if (board->pacmans) free(board->pacmans);
    if (board->ghosts) free(board->ghosts);
    board->board = NULL;
    board->pacmans = NULL;
    board->ghosts = NULL;
}