#include "include/inicializacion_master.h"

void inicializacion() {
    terminar=0; 
    generador_ids=0;
    master_logger = iniciar_logger("master.log","MASTER_LOG","DEBUG");
    master_config = iniciar_config(archivo_config);
    cargar_configs();
    fd_cliente = iniciar_servidor(atoi(PUERTO_ESCUCHA), master_logger, "MASTER");
    fd_interrupt = iniciar_servidor(atoi(PUERTO_ESCUCHA_INTERRUPT),master_logger, "INTERRUPT");
    //iniciando listas
    queries_ready =  list_create();
    queries_exit = list_create(); 
    queries_exec = list_create();
    workers_conectados = list_create();

    //iniciando mutex
    pthread_mutex_init(&mutex_generador, NULL);
    pthread_mutex_init(&mutex_lista_ready,NULL);
    pthread_mutex_init(&mutex_lista_exec,NULL);
    pthread_mutex_init(&mutex_lista_exit,NULL);
    pthread_mutex_init(&mutex_lista_workers, NULL);

    //iniciando semaforors 
    sem_init(&sem_hay_workes_disponibles,0,0);
    sem_init(&sem_hay_queries_en_ready,0,0);
    sem_init(&sem_hay_queries_en_exit,0,0);
}

void cargar_configs(){
    PUERTO_ESCUCHA = config_get_string_value(master_config,"PUERTO_ESCUCHA"); 
    ALGORITMO_PLANIFICACION = config_get_string_value(master_config,"ALGORITMO_PLANIFICACION"); 
    TIEMPO_AGING =config_get_int_value(master_config,"TIEMPO_AGING");
    LOG_LEVEL= config_get_string_value(master_config,"LOG_LEVEL");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(master_config,"PUERTO_ESCUCHA_INTERRUPT");
}

void finalizador_de_estructuras() {
    free(LOG_LEVEL);
    free(PUERTO_ESCUCHA);
    free(PUERTO_ESCUCHA_INTERRUPT);
    log_destroy(master_logger);
    //config
    config_destroy(master_config); 
    //listas
    list_destroy_and_destroy_elements(workers_conectados,destruir_worker_conectada);
    list_destroy_and_destroy_elements(queries_exec,destruir_queries);
    list_destroy_and_destroy_elements(queries_exit,destruir_queries);
    list_destroy_and_destroy_elements(queries_ready,destruir_queries);
    //servidores
    close(fd_interrupt);
    close(fd_cliente);
    //inicar mutex y semaforos
    sem_destroy(&sem_hay_queries_en_exit);
    sem_destroy(&sem_hay_queries_en_ready);

    pthread_mutex_destroy(&mutex_generador);
    pthread_mutex_destroy(&mutex_lista_exec);
    pthread_mutex_destroy(&mutex_lista_exit);
    pthread_mutex_destroy(&mutex_lista_ready);
    pthread_mutex_destroy(&mutex_lista_workers);
}
