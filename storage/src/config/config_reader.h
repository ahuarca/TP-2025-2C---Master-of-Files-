#ifndef CONFIG_READER_H
#define CONFIG_READER_H
#include <commons/config.h>


typedef struct
{
    char* fresh_start; // lo cambio a string asi compila bien
    int puerto_escucha; // la funcion para obtener el socket server espera que el puerto sea char*
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;

    int initial_fs_size;
    int initial_block_size;
    t_config* config_bk;
}t_custom_config;


int read_config(t_custom_config *cfg, char *cfg_path);

#endif
