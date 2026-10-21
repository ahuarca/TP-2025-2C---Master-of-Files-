#ifndef OPERACIONES_H
#define OPERACIONES_H
#include <stdio.h>
#include "../filesystem/structures.h"

int create_file(char* file_name, char* tag_name, int pid);
int truncar_file(char* file_name, char* tag_name, int cant_bloques, int pid);
int copiar_tag(char* file_name, char* tag_name_src, char* tag_name_dst,char* nombre_nuevo_de_tag ,int pid);
int commit_tag(char* file_name, char* tag_name, int pid);
int delete_tag(char* file_name, char* tag_name, int pid);
int leer_bloque(char** out_content, char* file_name, char* tag_name, int block_number);
int escribir_bloque(char* file_name, char* tag_name, int logical_block_number, char* content, int pid);
void flush_file(char* nombre_file, char* nombre_tag, char* contenido_a_persistir, int num_bloque_logico);
int quitarHashDeLaConfigDeHashes(int num_bloque_fisico);
int buscarBloqueFisicoPorNombreEnConfigHash(char* nombreDelBloque);
char* read_all(FILE* f);
dir_lista_t restaurar_files_tags(const char* path_base);
void rellenar_diccionarios(const char*  path_base);
void agregarBloquesTagSegunMetadata(t_tag* tag);


#endif //OPERACIONES_H
