#ifndef OPERACIONES_FS_H
#define OPERACIONES_FS_H
#include "structures.h"
#include <pthread.h>
int crear_estructura_file(t_file* file_out, char* nombre_file, int pid_create, char* nombre_tag);
int crear_estructura_tag(t_tag* tag_out, char* nombre_file, char* nombre_tag, int pid);
int add_block_to_tag(char* file_name, char* tag_name, int block_number, int add_at_index, int pid);
int add_block_to_tag_en_write(char* file_name, char* tag_name, int num_bloque_fisico,int num_bloque_logico, int pid);
int write_to_block(char* file_name, char* tag_name, int logical_block_number, int physical_block_number, char* data);
int eliminar_bloque_logico(char* file_name, char* tag_name, int physical_block_num, int logical_block_num, int pid);
int update_tag_status(char* file_name, char* tag_name, char* status);
int reservar_bloques_por_primera_vez(int* bloques, int cant, int pid);
int reservar_bloques(int* bloques, int cant, int pid);

void disminuir_size_en_metadata_config(t_tag* tag, int cantidad);
void aumentar_size_en_metadata_config(t_tag* tag, int cantidad);
void aumentar_bloques_en_metadata_config(t_tag* tag, int cantidad);
void disminuir_bloques_en_metadata_config(t_tag* tag, int num_bloque_fisico, int num_bloque_logico);
char** quitar_bloque_del_array(char** array_anterior, char* num_bloque, int num_bloque_logico);
void aumentar_bloques_en_metadata_config_en_write(t_tag* tag, int num_bloque_fisico, int num_bloque_logico);
// Solo usar desde inicializacion
int setear_bloque_ocupado_bitmap(int num_bloque);
#endif //OPERACIONES_FS_H
