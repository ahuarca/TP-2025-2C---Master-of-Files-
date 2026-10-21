#include "include/planificador.h"

void* planificador_corto(){
    while(!terminar){
        sem_wait(&sem_hay_workes_disponibles);
        sem_wait(&sem_hay_queries_en_ready); 

        //busco la que esta disponible y validarlo 
        worker_t* worker = buscar_worker_libre();
        if(worker==NULL){
            log_info(master_logger,"se activa el semaforo que hay workes libres, pero no se encontro ninguna");
            continue; 
        }
        //log_debug(master_logger, "worker libre: %d", worker->id);

        //busco el proximo proceso a ejecutar teniendo en cuenta los dif algoritmos y validarlo
        query_t* query_proximo = buscar_proximo_query(); 

        if(query_proximo==NULL){
            log_error(master_logger,"se activa el semaforo que hay queries en READY, pero no se encontro ninguno");
            continue; 
        } 
        
        //log_debug(master_logger, "proxima query %d", query_proximo->id);

        /*
        if(strcmp(ALGORITMO_PLANIFICACION,"PRIORIDADES")==0){
            //para que me termine el cronometro del aging y no me baje la prioridad
            log_debug(master_logger,"termino el temporizador de aging de query %d", query_proximo->id);
            sem_post(&query_proximo->sem_aging);
        }
        */

        pasar_query_de_estado_a_otro(query_proximo,READY,EXEC);

        pthread_mutex_lock(&mutex_lista_exec);
        query_proximo->ultimoWorker = worker->id;
        pthread_mutex_unlock(&mutex_lista_exec);
       
        //marco el worker como ocupad0
        pthread_mutex_lock(&mutex_lista_workers);
        worker->estaLibre =false;
        worker->query_tomada=query_proximo->id;
        pthread_mutex_unlock(&mutex_lista_workers);

        //le mando un msj al worker 
        log_info(master_logger,"## Se envía la Query <%d> al Worker <%d>", query_proximo->id, worker->id);
        mandar_mensaje_a_worker(worker, query_proximo);

    }
    return NULL;
}


//FINALIDAOR DE QUERIES 
void* finalizador() {
     while(!terminar){
        sem_wait(&sem_hay_queries_en_exit); 

        pthread_mutex_lock(&mutex_lista_exit);
        query_t* query = list_get(queries_exit,0);
        list_remove_element(queries_exit, query);
        pthread_mutex_unlock(&mutex_lista_exit);

        log_info(master_logger, "## Se terminó la Query <%d> en el Worker <%d>", query->id, query->ultimoWorker);

        //le aviso a la query q finaliza y espero su confimacion
        mensaje_al_query_control(FIN_MASTER_QUERY_C,query);
    }
    return NULL;
}