#ifndef MAIN_MASTER_H_
#define MAIN_MASTER_H_


#include "inicializacion_master.h"
#include "conexiones_con_modulos.h"
#include "planificador.h"
#include "utils_de_master.h"

char* archivo_config;
t_log* master_logger;
t_config* master_config;

char* PUERTO_ESCUCHA;
char* ALGORITMO_PLANIFICACION ;
int TIEMPO_AGING;
char* LOG_LEVEL;
char* PUERTO_ESCUCHA_INTERRUPT;

int generador_ids;
int fd_cliente;
int fd_interrupt;

t_list* queries_ready;
t_list* queries_exit; 
t_list* queries_exec;
t_list* workers_conectados;

pthread_mutex_t mutex_generador;
pthread_mutex_t mutex_lista_ready;
pthread_mutex_t mutex_lista_exec;
pthread_mutex_t mutex_lista_exit;
pthread_mutex_t mutex_lista_workers;

sem_t sem_hay_workes_disponibles;
sem_t sem_hay_queries_en_ready;
sem_t sem_hay_queries_en_exit;

volatile sig_atomic_t terminar;


#endif