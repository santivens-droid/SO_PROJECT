#ifndef LOADER_H
#define LOADER_H

#include "board.h"

/* Carrega um nível a partir de ficheiros para a estrutura board.
   Retorna 0 em caso de sucesso, -1 em erro.
*/
int load_level(board_t* board, const char* dir_path, const char* level_file, int accumulated_points);

/* Liberta a memória alocada pelo load_level 
*/
void unload_level(board_t * board);

#endif