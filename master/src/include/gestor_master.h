
#ifndef GESTOR_MASTER_H_
#define GESTOR_MASTER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <commons/temporal.h>

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>




#include "../../../utils/src/utils/conexiones.h" 

typedef enum{
    READY,
    EXEC,
    EXIT
}estados;
typedef struct{
    int id;
    char* path;
    int fd_queryControl;
    int prioridad;
    int ultimoWorker;
    estados estado;
    int pc;
    sem_t sem_aging; //me falta implementacion
    sem_t sincronizacion_cronometro; //todavia no esta implementado
}query_t;

typedef struct{
    int id;
    int fd_worker;
    bool estaLibre;
    int fd_worker_interrupt;
    int query_tomada;
}worker_t;

extern char* archivo_config;
extern t_log* master_logger;
extern t_config* master_config;

extern char* PUERTO_ESCUCHA;
extern char* ALGORITMO_PLANIFICACION ;
extern int TIEMPO_AGING;
extern char* LOG_LEVEL;
extern char* PUERTO_ESCUCHA_INTERRUPT;

extern t_list* queries_ready;
extern t_list* queries_exit; 
extern t_list* queries_exec;
extern t_list* workers_conectados;

extern pthread_mutex_t mutex_generador;
extern pthread_mutex_t mutex_lista_ready;
extern pthread_mutex_t mutex_lista_exec;
extern pthread_mutex_t mutex_lista_exit;
extern pthread_mutex_t mutex_lista_workers;

extern sem_t sem_hay_workes_disponibles;
extern sem_t sem_hay_queries_en_ready;
extern sem_t sem_hay_queries_en_exit; 

extern int generador_ids;
extern int fd_cliente;
extern int fd_interrupt;

extern volatile sig_atomic_t terminar;


#endif