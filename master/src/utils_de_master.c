#include "include/utils_de_master.h"
#include <commons/string.h>

void agregar_a_ready(query_t* query){
    if(strcmp("FIFO", ALGORITMO_PLANIFICACION)==0){
        pthread_mutex_lock(&mutex_lista_ready);
        list_add(queries_ready, query);
        query->estado =READY;
        pthread_mutex_unlock(&mutex_lista_ready);
    }else if(strcmp("PRIORIDADES", ALGORITMO_PLANIFICACION)==0){
        //log_debug(master_logger,"es con prioridades");
        agregar_acorde_a_la_prioridad(query);
        bool huboInterrupcion = chequeo_de_desalojo(query);

        if(huboInterrupcion==false){
            while (sem_trywait(&query->sem_aging) == 0);

            pthread_t hilo_cambio_de_prioridades;
            pthread_create(&hilo_cambio_de_prioridades,NULL, contador_de_cambio_de_prioridad, (void*)query);
            pthread_detach(hilo_cambio_de_prioridades);
            //log_debug(master_logger, "NO HUBO DESALOJO => arranco hilo para el aging para query %d", query->id);

            sem_wait(&query->sincronizacion_cronometro); //wait que se active una vez que se sincronicen los hilos
            //log_debug(master_logger, "se hizo el sincronismo entre agregar a ready y hilo aging para query %d", query->id);
        }
    }else{
        log_debug(master_logger, "algoritmo erroneo");
    }
}

void agregar_acorde_a_la_prioridad(query_t* query){
    bool menor_id(void* elem1, void* elem2){
        query_t* query1 =(query_t*)elem1;
        query_t* query2 =(query_t*)elem2;
        return query1->prioridad <= query2->prioridad;
    };
    pthread_mutex_lock(&mutex_lista_ready);
    list_add_sorted(queries_ready,query, menor_id);
    query->estado =READY;
    pthread_mutex_unlock(&mutex_lista_ready);
}

int calcular_nivel_multiprogramacion(){
    pthread_mutex_lock(&mutex_lista_workers);
    int cantidad = list_size(workers_conectados);
    pthread_mutex_unlock(&mutex_lista_workers);

    return cantidad;
}

query_t* buscar_query_con_untimoWorker(int id_worker){
     bool query_con_id(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->ultimoWorker==id_worker;
    };
    pthread_mutex_lock(&mutex_lista_exec);
    query_t* query = (query_t*) list_find(queries_exec, query_con_id);
    pthread_mutex_unlock(&mutex_lista_exec);
    
    return query;
}


worker_t* buscar_worker_libre(){
    bool worker_libre(void* ptr) {
        worker_t* worker = (worker_t*) ptr;
        return worker->estaLibre==true;
    };
    pthread_mutex_lock(&mutex_lista_workers);
    worker_t* worker = (worker_t*) list_find(workers_conectados, worker_libre);
    pthread_mutex_unlock(&mutex_lista_workers);
    
    return worker;
}

query_t* buscar_proximo_query(){
    pthread_mutex_lock(&mutex_lista_ready);
    query_t* query = (query_t*) list_get(queries_ready, 0);
    pthread_mutex_unlock(&mutex_lista_ready);

    return query;

}

void pasar_query_de_estado_a_otro(query_t* query_proximo,estados inicio,estados fin){
    sacar_query_de_estado(query_proximo, inicio);
    poner_query_en_estado(query_proximo, fin);
}

void sacar_query_de_estado(query_t* query, estados estado){
    switch(estado){
        case(READY):
            if(strcmp("PRIORIDADES",ALGORITMO_PLANIFICACION)==0){
                //log_debug(master_logger,"termino el temporizador de aging de query %d", query->id);
                sem_post(&query->sem_aging);
            }
            pthread_mutex_lock(&mutex_lista_ready);
            list_remove_element(queries_ready, query);
            pthread_mutex_unlock(&mutex_lista_ready);
            //log_debug(master_logger, "saco query %d de READY", query->id);
        break;
        case(EXEC):
            pthread_mutex_lock(&mutex_lista_exec);
            list_remove_element(queries_exec, query);
            pthread_mutex_unlock(&mutex_lista_exec);
            //log_debug(master_logger, "saco query %d de EXEC", query->id);
        break;
        case(EXIT):
            pthread_mutex_lock(&mutex_lista_exit);
            list_remove_element(queries_exit, query);
            pthread_mutex_unlock(&mutex_lista_exit);
            //log_debug(master_logger, "saco query %d de EXIT", query->id);
        break;
        default:
            log_debug(master_logger, "estado incorrecto");
        break;
    }
}

void poner_query_en_estado(query_t* query, estados estado){
    switch(estado){
        case(READY):
            agregar_a_ready(query);
            //log_debug(master_logger, "pongo query %d en READY", query->id);
        break;
        case(EXEC):
            pthread_mutex_lock(&mutex_lista_exec);
            list_add(queries_exec, query);
            query->estado = EXEC;
            pthread_mutex_unlock(&mutex_lista_exec);
            //log_debug(master_logger, "pongo query %d en EXEC", query->id);
        break;
        case(EXIT):
            pthread_mutex_lock(&mutex_lista_exit);
            list_add(queries_exit, query);
            query->estado=EXIT;
            pthread_mutex_unlock(&mutex_lista_exit);
            //log_debug(master_logger, "pongo query %d en EXIT", query->id);
        break;
        default:
            log_debug(master_logger, "estado incorrecto");
        break;
    }
}

void mandar_mensaje_a_worker(worker_t* worker, query_t* query){
    t_buffer* buffer = crear_buffer();
    cargar_entero_al_buffer(buffer, query->id);
    cargar_entero_al_buffer(buffer, query->pc);
    cargar_string_al_buffer(buffer, query->path);
    t_paquete* paquete = crear_paquete(EJECUTAR_QUERY,buffer);
    enviar_paquete(paquete, worker->fd_worker);
    eliminar_paquete(paquete);
}

void mensaje_al_query_control(op_code operacion, query_t* query){
    switch(operacion){
        case(FIN_MASTER_QUERY_I):
            //log_debug(master_logger, "mando msj a query %d por finaliacion INCORRECTA", query->id);
            t_buffer* buffer = crear_buffer();
            cargar_string_al_buffer(buffer,"desconexion del worker");
            t_paquete* paquete = crear_paquete(FIN_MASTER_QUERY,buffer);
            enviar_paquete(paquete, query->fd_queryControl);
            eliminar_paquete(paquete);
        break;
        case(FIN_MASTER_QUERY_C):
            //log_debug(master_logger, "mando msj a query %d por finaliacion CORRECTA", query->id);
            t_buffer* buffer2 = crear_buffer();
            cargar_string_al_buffer(buffer2,"correcta finalizacion del query");
            t_paquete* paquete2 = crear_paquete(FIN_MASTER_QUERY,buffer2);
            enviar_paquete(paquete2, query->fd_queryControl);
            eliminar_paquete(paquete2);
        break;
        default:
            log_info(master_logger, "operacion incorrecta para query");
        break;
    }

}

//DESALOJO
bool chequeo_de_desalojo(query_t* query){
    if(buscar_worker_libre()==NULL){
        query_t* query_a_cambiar = query_con_menos_prioridad_en_worker(query->prioridad);
        if(query_a_cambiar!=NULL){
            log_info(master_logger, "## Se desaloja la Query <%d> (<%d>) del Worker <%d> - Motivo: <PRIORIDAD>",query_a_cambiar->id, query_a_cambiar->prioridad, query_a_cambiar->ultimoWorker);
            mensaje_de_desalojo(query_a_cambiar);
            //log_debug(master_logger,"se mando msj desalojo");
            return true;
        }else{
            return false;
        }
    }
}

query_t* query_con_menos_prioridad_en_worker(int prioridad){
    bool query_con_menos_prioridad(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->prioridad > prioridad;
    };
    pthread_mutex_lock(&mutex_lista_exec);
    query_t* query = (query_t*) list_find(queries_exec, query_con_menos_prioridad);
    pthread_mutex_unlock(&mutex_lista_exec);
    
    return query;
}

worker_t* buscar_worker_con_id(int id_worker){
    bool buscar_worker(void* ptr) {
        worker_t* worker = (worker_t*) ptr;
        return worker->id ==id_worker;
    };
    pthread_mutex_lock(&mutex_lista_workers);
    worker_t* worker = (worker_t*) list_find(workers_conectados, buscar_worker);
    pthread_mutex_unlock(&mutex_lista_workers);
    
    return worker;
}

query_t* buscar_query_con_id_en(int pid, estados estado){
    bool buscar_query(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->id == pid;
    };
    query_t* queryDestino;
    if(estado == READY){
        pthread_mutex_lock(&mutex_lista_ready);
        queryDestino= (query_t*) list_find(queries_ready, buscar_query);
        pthread_mutex_unlock(&mutex_lista_ready);
    }else if(estado==EXEC){
        pthread_mutex_lock(&mutex_lista_exec);
        queryDestino= (query_t*) list_find(queries_exec, buscar_query);
        pthread_mutex_unlock(&mutex_lista_exec);
    }else if(estado==EXIT){
        pthread_mutex_lock(&mutex_lista_exit);
        queryDestino= (query_t*) list_find(queries_exit, buscar_query);
        pthread_mutex_unlock(&mutex_lista_exit);
    }
    return queryDestino;
    
}

void mensaje_de_desalojo(query_t* query_cambiar){
    worker_t* worker = buscar_worker_con_id(query_cambiar->ultimoWorker);

    t_buffer* buffer = crear_buffer();
    cargar_entero_al_buffer(buffer,query_cambiar->id);
    t_paquete* paquete = crear_paquete(DESALOJAR_QUERY,buffer);
    enviar_paquete(paquete, worker->fd_worker_interrupt);

    eliminar_paquete(paquete);
}


//////AGING//////////
void* contador_de_cambio_de_prioridad(void* arg){
    query_t* query = (query_t*) arg;
    struct timespec ts, fin , inicio;
    clock_gettime(CLOCK_REALTIME, &inicio);
    
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += (TIEMPO_AGING / 1000);
    ts.tv_nsec += ((TIEMPO_AGING % 1000) * 1000000);
    
    // Normalizar los nanosegundos en caso de overflow
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
    sem_post(&query->sincronizacion_cronometro); //para sincronizar hilos
    //log_debug(master_logger, "sincronismo de hilos");

    int resultado = sem_timedwait(&query->sem_aging, &ts); 
    //agrego para verificar cuanto espera 
    clock_gettime(CLOCK_REALTIME, &fin); 

    // Calcular diferencia en milisegundos
    long tiempo_esperado_ms = (fin.tv_sec - inicio.tv_sec) * 1000 + 
                              (fin.tv_nsec - inicio.tv_nsec) / 1000000;

    if (resultado == -1 && errno == ETIMEDOUT){
        //se cumplio el tiempo y sigue en ready
        if(sigue_en_ready(query)){
            if (query->prioridad > 0){
                pthread_mutex_lock(&mutex_lista_ready);
                int ant = query->prioridad;
                query->prioridad--;
                pthread_mutex_unlock(&mutex_lista_ready);
                log_info(master_logger, "##<%d> Cambio de prioridad: <%d> - <%d>", query->id, ant, query->prioridad);

                //ACA TENDRIA QUE SACARLO DE READY Y VOLVERLO A PONER ASI SE REORGANIZA CON LA NUEVA PRIORIDAD, CHECKEA INTERRUPCION Y EMPIEZA EL CONTADOR DEVUELTA 
                sacar_query_de_estado(query,READY);
                poner_query_en_estado(query,READY);
            }else{
                //log_debug(master_logger,"no le bajo la prioridad porque es cero query %d", query->id);
            }

        }
    }
    return NULL;
}

bool sigue_en_ready(query_t* query){
    bool mismo_id(void* ptr) {
        query_t* query_p = (query_t*) ptr;
        return query_p->id ==query->id;
    };

    pthread_mutex_lock(&mutex_lista_ready);
    bool rta = list_any_satisfy(queries_ready, mismo_id);
    pthread_mutex_unlock(&mutex_lista_ready);
    
    return rta;
}

void liberar_query (query_t* query) {
    free(query->path);
    free(query);
}
void liberar_worker(worker_t* worker){
    free(worker);
}


void sacar_query_de_algun_estado(query_t* query){
    switch(query->estado){
        case(READY):
            sacar_query_de_estado(query, READY);
            poner_query_en_estado(query,EXIT);
            sem_post(&sem_hay_queries_en_exit);
        break;
        case(EXEC):
            mensaje_de_desalojo_por_desconexion(query);
            log_info(master_logger,"## Se desaloja la Query <%d> (<%d>) del Worker <%d> - Motivo: <DESCONEXION>", query->id, query->prioridad, query->ultimoWorker);
        break;
        case(EXIT):
        break;
        default:
            log_error(master_logger,"accion erronea");
        break;
    }
}


void mensaje_de_desalojo_por_desconexion(query_t* query_cambiar){
    worker_t* worker = buscar_worker_con_id(query_cambiar->ultimoWorker);

    t_buffer* buffer = crear_buffer();
    cargar_entero_al_buffer(buffer,query_cambiar->id);
    t_paquete* paquete = crear_paquete(DESALOJAR_QUERY_DESCONEXION,buffer);
    enviar_paquete(paquete, worker->fd_worker_interrupt);

    eliminar_paquete(paquete);
}



//me parece que no se usan mas 
bool esta_query_en_READY(int id){
   bool mismo_id(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->id == id;
    };
    pthread_mutex_lock(&mutex_lista_ready);
    bool rta = list_any_satisfy(queries_ready, mismo_id);
    pthread_mutex_unlock(&mutex_lista_ready);
    
    return rta;
    
}
bool esta_query_en_EXIT(int id){
     bool mismo_id(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->id == id;
    };
    pthread_mutex_lock(&mutex_lista_exit);
    bool rta = list_any_satisfy(queries_exit, mismo_id);
    pthread_mutex_unlock(&mutex_lista_exit);
    
    return rta;
}
bool esta_query_en_EXEC(int id){
     bool mismo_id(void* ptr) {
        query_t* query = (query_t*) ptr;
        return query->id == id;
    };
    pthread_mutex_lock(&mutex_lista_exec);
    bool rta = list_any_satisfy(queries_exec, mismo_id);
    pthread_mutex_unlock(&mutex_lista_exec);
    
    return rta;
}

void destruir_worker_conectada(void* workerV){
    worker_t* worker = (worker_t*) workerV;
    close(worker->fd_worker);
    close(worker->fd_worker_interrupt);
    free(worker);
}
void destruir_queries(void* queryV) {
    query_t* query = (query_t*)queryV;
    free(query->path);
    close(query->fd_queryControl);
    sem_destroy(&(query->sem_aging));
    sem_destroy(&(query->sincronizacion_cronometro));
    free(query);
}

void manejar_ctrl_c(int sig) {
    switch (sig)
    {
    case SIGINT:
        terminar =1;
        log_debug(master_logger,"\n[Master] Señal SIGINT capturada. Finalizando...\n");
        finalizador_de_estructuras();
        break;
    default:
        log_warning(master_logger,"senial no manejada");
        break;
    }
     exit(EXIT_SUCCESS);
}