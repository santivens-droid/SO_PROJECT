#ifndef FILE_PARSER_H
#define FILE_PARSER_H

#include "board.h" // Necessário para aceder a board_t, MAX_FILENAME e MAX_LEVELS

/**
 * @brief Lê uma diretoria e encontra todos os ficheiros com extensão .lvl.
 * * Esta função varre a diretoria especificada, filtra apenas os ficheiros 
 * que terminam em ".lvl", constrói o caminho completo para cada um e 
 * guarda-os no array fornecido. No final, ordena a lista alfabeticamente.
 * * @param dir_path O caminho para a diretoria a pesquisar (ex: "niveis/").
 * @param levels Array onde serão guardados os caminhos completos dos ficheiros.
 * @return int O número total de níveis encontrados e guardados.
 */
int get_sorted_levels(const char *dir_path, char levels[MAX_LEVELS][MAX_FILENAME]);

/**
 * @brief Carrega o conteúdo de um nível a partir de um ficheiro .lvl.
 * * Esta função lê o ficheiro usando chamadas de sistema POSIX (open, read, close),
 * processa os cabeçalhos (DIM, TEMPO, PAC, MON) e preenche a grelha do jogo
 * (paredes, pontos, portais, posição inicial do Pacman).
 * * @param board Apontador para a estrutura do jogo (board_t) a ser preenchida.
 * @param level_path Caminho completo para o ficheiro .lvl a carregar.
 * @param accumulated_points Pontos que o Pacman traz do nível anterior (para persistência).
 * @return int 0 em caso de sucesso, -1 em caso de erro (ex: ficheiro não encontrado).
 */
int load_level_from_file(board_t* board, const char* level_path, int accumulated_points);

#endif // FILE_PARSER_H