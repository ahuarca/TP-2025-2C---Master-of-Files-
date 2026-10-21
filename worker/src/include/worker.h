#ifndef WORKER_H_
#define WORKER_H_

#include "./inicializador.h"

// Parametros

char* archivo_config;
int id_worker;

// Datos de la config

t_log* worker_logger;
t_config* worker_config;

char* IP_MASTER;
char* PUERTO_MASTER;
char* PUERTO_MASTER_INTERRUPT;
char* IP_STORAGE;
char* PUERTO_STORAGE;
int TAM_MEMORIA;
int RETARDO_MEMORIA;
char* ALGORITMO_REEMPLAZO;
char* PATH_SCRIPTS;
char* LOG_LEVEL;

// Conexiones con los modulos

int fd_storage;

int fd_master;
int fd_master_interrupt;

// Datos recibidos de storage

int TAM_PAGINA;

// Hilos

pthread_t atencion_interrupt;

// Funcion de libreria query_interpreter

void iniciar_query_interpreter();

// Memoria interna

char* memoria_interna;

#endif