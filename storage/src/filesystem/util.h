#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>

#include "../filesystem/structures.h"
#include <stdio.h>

int get_path_by_file_name(char* path, char* file_name);
int get_path_by_file_name_and_tag(char* path, char* file_name, char* tag_name);
int get_logical_blocks_path_by_file_name_and_tag_name(char* path, char* file_name, char* tag_name);
int get_metadata_config_path_by_file_name_and_tag_name(char* path, char* file_name, char* tag_name);
int get_logical_block_path(char* path, char* file_name, char* tag_name, const int num_bloque_logico);
int crear_directorio(char* path);
char* block_num_to_block_name(int num);
char* hash_bloque(int num_bloque);
int read_block(char** out, int num_bloque);
int eliminar_archivos_de_directorio(char* ruta_dir);
bool directorio_existe(char *ruta_de_directorio);
int generar_hard_link(char* ruta_del_origen, char* ruta_destino);
int set_block_hash_in_map(int block_number, char* hash);
char *join_strings(char **arr, size_t count);
int sumar_inode_de_bloque(int block_number);
int restar_inode_de_bloque(int block_number);
int bloque_duplicado(int block_number);
char* read_all(FILE* f);
int cant_hardlinks_de_bloque(const int block_number);
void liberar_tag(t_tag* tag_a_eliminar);
void liberar_bloque(void* ptr);
int obtenerCantHardLinks(const int block_number);
#endif //UTIL_H
