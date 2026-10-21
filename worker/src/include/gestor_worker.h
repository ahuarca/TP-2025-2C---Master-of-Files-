#ifndef GESTOR_WORKER_H_
#define GESTOR_WORKER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include "../../utils/src/utils/conexiones.h"
#include <commons/temporal.h>


typedef struct{
    int nro_pagina;
    int ocupado;
    int proceso_id;
    t_temporal* ultima_referencia;
}t_marco;
// Parametros

extern char* archivo_config;
extern int id_worker;
extern pthread_mutex_t procesos_mutex;
extern t_marco* puntero_clock;
extern t_marco* puntero_flush;
extern int puntero_index;
extern t_list* procesos;
extern int CANT_MARCOS;

// Datos de la config

extern t_log* worker_logger;
extern t_config* worker_config;

extern pthread_t atencion_interrupt;

extern char* IP_MASTER;
extern char* PUERTO_MASTER;
extern char* PUERTO_MASTER_INTERRUPT;
extern char* IP_STORAGE;
extern char* PUERTO_STORAGE;
extern int TAM_MEMORIA;
extern int RETARDO_MEMORIA;
extern char* ALGORITMO_REEMPLAZO;
extern char* PATH_SCRIPTS;
extern char* LOG_LEVEL;

// Conexiones con los modulos

extern int fd_storage;

extern int fd_master;
extern int fd_master_interrupt;

// Datos recibidos de storage

extern int TAM_PAGINA;

// Memoria Interna

extern char* memoria_interna;
#endif