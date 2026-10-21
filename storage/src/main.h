#ifndef STORAGE_H_
#define STORAGE_H_
#include <stdio.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include "config/config_reader.h"
#include "filesystem/structures.h"

extern t_config* config_hash;
extern t_custom_config* configuration;
extern t_config_super_block* super_config;
extern t_log* logger;
//socket server
extern int fd_server;
// Diccionarios
extern t_dictionary* dict_files;
//listas
extern t_list* lista_bloques_fisicos;
extern t_list* lista_workers;
//variables globales
extern int TERMINAR;
extern int CANT_WORKERS_CONECTADOS;
extern int CANT_BLOQUES;
extern int fd_bitmap;
// Bitmap bloques
extern void* archivo_bitmap_bloques_mapeado;
extern t_bitarray* bitarray_bloques;
//archivos
extern FILE* archivo_block_hash_index;
//mutex
extern pthread_mutex_t mutexCantWorkers;
extern pthread_mutex_t mutexConfigHash;
extern pthread_mutex_t mutexListaBloquesFisicos;
extern pthread_mutex_t mutexDirectorioFiles;
extern pthread_mutex_t mutexBitarrayBloques;

#endif