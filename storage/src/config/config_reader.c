#include "config_reader.h"
#include <commons/config.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int read_config(t_custom_config *cfg, char *cfg_path) {

    t_config *configuration = config_create(cfg_path);

    if (configuration == NULL)
    {
        printf("Error creating config");
        return -2;
    }
    if (!config_has_property(configuration, "PUERTO_ESCUCHA")) {
        printf("Error reading PUERTO_ESCUCHA");
        config_destroy(configuration);
        return -1;
    }
    cfg->puerto_escucha = config_get_int_value(configuration, "PUERTO_ESCUCHA");

    if (!config_has_property(configuration, "FRESH_START")) {
        printf("Error reading FRESH_START");
        config_destroy(configuration);
        return -1;
    }
    cfg->fresh_start = config_get_string_value(configuration, "FRESH_START");

    if (!config_has_property(configuration, "PUNTO_MONTAJE")) {
        printf("Error reading PUNTO_MONTAJE");
        config_destroy(configuration);
        return -1;
    }
    cfg->punto_montaje = config_get_string_value(configuration, "PUNTO_MONTAJE");

    if (!config_has_property(configuration, "RETARDO_OPERACION")) {
        printf("Error reading RETARDO_OPERACION");
        config_destroy(configuration);
        return -1;
    }
    cfg->retardo_operacion = config_get_int_value(configuration, "RETARDO_OPERACION");

    if (!config_has_property(configuration, "RETARDO_ACCESO_BLOQUE")) {
        printf("Error reading RETARDO_ACCESO_BLOQUE");
        config_destroy(configuration);
        return -1;
    }
    cfg->retardo_acceso_bloque = config_get_int_value(configuration, "RETARDO_ACCESO_BLOQUE");

    if (!config_has_property(configuration, "LOG_LEVEL")) {
        printf("Error reading LOG_LEVEL");
        config_destroy(configuration);
        return -1;
    }
    cfg->log_level = config_get_string_value(configuration, "LOG_LEVEL");

    // LECTURA PARAMETROS SUPERBLOCK DEFAULT
    if (!config_has_property(configuration, "FS_SIZE")) {
        printf("Error reading FS_SIZE");
        config_destroy(configuration);
        return -1;
    }
    cfg->initial_fs_size = config_get_int_value(configuration, "FS_SIZE");

    if (!config_has_property(configuration, "BLOCK_SIZE")) {
        printf("Error reading BLOCK_SIZE");
        config_destroy(configuration);
        return -1;
    }
    cfg->initial_block_size = config_get_int_value(configuration, "BLOCK_SIZE");
    // Eliminar config auxiliar
    cfg->config_bk = configuration;
    return 0;
}

