#include "include/conexiones_con_modulos.h"

//conexiones con modulos 
void* conexiones_con_clientes() {
    while(1){
        int fd_client = esperar_cliente(fd_cliente);
        if(fd_client==-1){
            continue;
        }else{
            op_code codigo= recibir_operacion(fd_client);
            switch(codigo){
                case(HANDSHAKE_QUERY_MASTER): 
                    log_info(master_logger,"## Se conecto nuevo queryControl");
                    t_buffer* buffer = recibir_todo_un_buffer(fd_client);
                    char* path = extraer_string_del_buffer(buffer);
                    int prioridad = extraer_int_del_buffer(buffer);

                    char* copia = malloc(strlen(path)+1);
                    strcpy(copia,path);
                    free(path);

                    query_t* query_nueva = malloc(sizeof(query_t));
                    pthread_mutex_lock(&mutex_generador);
                    query_nueva->id = generador_ids;
                    pthread_mutex_unlock(&mutex_generador);
                    query_nueva->fd_queryControl = fd_client;
                    query_nueva->path = copia;
                    query_nueva->prioridad = prioridad;
                    query_nueva->ultimoWorker = -1;
                    query_nueva->pc =0;
                    sem_init(&query_nueva->sincronizacion_cronometro, 0, 0);
                    sem_init(&query_nueva->sem_aging, 0, 0);

                    int nivelMultiprogramacion = calcular_nivel_multiprogramacion();

                    log_info(master_logger,"## Se conecta un Query Control para ejecutar la Query <%s> con prioridad <%d> - Id asignado: <%d>. Nivel multiprogramación <%d>",copia, prioridad, query_nueva->id, nivelMultiprogramacion);

                    poner_query_en_estado(query_nueva,READY);

                    sem_post(&sem_hay_queries_en_ready);
                    pthread_mutex_lock(&mutex_generador);
                    generador_ids++;
                    pthread_mutex_unlock(&mutex_generador);
                    destruir_buffer(buffer);

                    pthread_t esperar_rtas_de_queryControl;
                    pthread_create(&esperar_rtas_de_queryControl ,NULL, espera_de_rta_de_queryControl , (void*)query_nueva);
                    pthread_detach(esperar_rtas_de_queryControl);
                    break;

                case(HANDSHAKE_WORKER_MASTER): 
                    log_info(master_logger,"Se conecto nuevo Worker");
                    t_buffer* buffer2 = recibir_todo_un_buffer(fd_client);

                    worker_t* worker_nuevo = malloc(sizeof(worker_t));
                    worker_nuevo->id = extraer_int_del_buffer(buffer2);;
                    worker_nuevo->estaLibre = true;
                    worker_nuevo->fd_worker = fd_client;
                    worker_nuevo->query_tomada = -1;

                    //aca esperaria la conexion al otro puerto 
                    int fd_interrup = esperar_cliente(fd_interrupt);
                    op_code codigo = recibir_operacion(fd_interrup);
                    if(codigo==HANDSHAKE_WORKER_MASTER_INTERRUPT){
                         t_buffer* buffer3 = recibir_todo_un_buffer(fd_interrup);
                         int id = extraer_int_del_buffer(buffer3);
                         if(id == worker_nuevo->id){
                             worker_nuevo->fd_worker_interrupt = fd_interrup;
                        }else{
                            log_info(master_logger, "algo raro paso con la conexion con worker");
                        }
                        destruir_buffer(buffer3);
                    }

                    //se agrega a la lista de workers
                    pthread_mutex_lock(&mutex_lista_workers); 
                    list_add(workers_conectados, worker_nuevo);
                    pthread_mutex_unlock(&mutex_lista_workers); 

                    int cantidad = calcular_nivel_multiprogramacion();
                    log_info(master_logger,"## Se conecta el Worker <%d> - Cantidad total de Workers: <%d>", worker_nuevo->id, cantidad);

                    sem_post(&sem_hay_workes_disponibles);
                    destruir_buffer(buffer2);

                    pthread_t esperar_rtas_de_worker;
                    pthread_create(&esperar_rtas_de_worker ,NULL, espera_de_rta_de_worker , (void*) worker_nuevo);
                    pthread_detach(esperar_rtas_de_worker);
                    break;
                default: 
                    log_info(master_logger,"MESAJE INCORRECTO");
                    break;
            }
        }
    }
}

void* espera_de_rta_de_worker(void* workerr){
    worker_t* worker = (worker_t*) workerr;
    int i=0;
    while(i==0){
        int codigo = recibir_operacion(worker->fd_worker);
        switch(codigo){
            case(-1):
                //saca al worker de los conectados 
                pthread_mutex_lock(&mutex_lista_workers);
                list_remove_element(workers_conectados, worker);
                pthread_mutex_unlock(&mutex_lista_workers);
                int cantidad = calcular_nivel_multiprogramacion();

                //saca al query asignada o ultuma query y lo finaliza 
                query_t* query = buscar_query_con_id_en(worker->query_tomada, EXEC);
                sacar_query_de_estado(query, EXEC);
                poner_query_en_estado(query,EXIT);

                log_info(master_logger,"## Se desconecta el Worker <%d> - Se finaliza la Query <%d> - Cantidad total de Workers: <%d>", worker->id, query->id, cantidad); 
                mensaje_al_query_control(FIN_MASTER_QUERY_I, query);

                liberar_worker(worker);
                i=1;
            break;
            case(SOL_END):
                t_buffer* buffer = recibir_todo_un_buffer(worker->fd_worker);
                int pid = extraer_int_del_buffer(buffer);
                int pc = extraer_int_del_buffer(buffer);

                query_t* query3 = buscar_query_con_id_en(pid, EXEC);
                pthread_mutex_lock(&mutex_lista_exec);
                query3->pc = pc;
                pthread_mutex_unlock(&mutex_lista_exec);

                pthread_mutex_lock(&mutex_lista_workers);
                worker->estaLibre=true;
                //worker->query_tomada=-1;
                pthread_mutex_unlock(&mutex_lista_workers);

                pasar_query_de_estado_a_otro(query3,EXEC,EXIT);
                sem_post(&sem_hay_queries_en_exit);
                sem_post(&sem_hay_workes_disponibles);

                eliminar_buffer(buffer);
            break;
            case(RTA_DESALOJO):
                t_buffer* buffer2 = recibir_todo_un_buffer(worker->fd_worker);
                int pid2 = extraer_int_del_buffer(buffer2);
                int pc2 = extraer_int_del_buffer(buffer2);
                destruir_buffer(buffer2);

                query_t* query2 = buscar_query_con_id_en(pid2,EXEC);
                pthread_mutex_lock(&mutex_lista_exec);
                query2->pc = pc2;
                pthread_mutex_unlock(&mutex_lista_exec);

                pasar_query_de_estado_a_otro(query2,EXEC,READY);

                pthread_mutex_lock(&mutex_lista_workers);
                worker->estaLibre=true;
                //worker->query_tomada=-1;
                pthread_mutex_unlock(&mutex_lista_workers);

                sem_post(&sem_hay_queries_en_ready);
                sem_post(&sem_hay_workes_disponibles);
            break;
              case(RTA_DESALOJO_DESCONEXION):
                t_buffer* buffer6 = recibir_todo_un_buffer(worker->fd_worker);
                int pid6 = extraer_int_del_buffer(buffer6);
                int pc6 = extraer_int_del_buffer(buffer6);
                destruir_buffer(buffer6);

                query_t* query6 = buscar_query_con_id_en(pid6,EXEC);
                pthread_mutex_lock(&mutex_lista_exec);
                query6->pc = pc6;
                pthread_mutex_unlock(&mutex_lista_exec);

                pasar_query_de_estado_a_otro(query6,EXEC,EXIT);

                pthread_mutex_lock(&mutex_lista_workers);
                worker->estaLibre=true;
                //worker->query_tomada=-1;
                pthread_mutex_unlock(&mutex_lista_workers);

                sem_post(&sem_hay_queries_en_exit);
                sem_post(&sem_hay_workes_disponibles);
            break;
            case(LECTURA_WORKER_MASTER):
                t_buffer* buffer5 = recibir_todo_un_buffer(worker->fd_worker);
                int query_id5 = extraer_int_del_buffer(buffer5);
                query_t* query5 = buscar_query_con_id_en(query_id5,EXEC);
                
                log_info(master_logger,"## Se envía un mensaje de lectura de la Query <%d> en el Worker <%d> al Query Control",query5->id, worker->id);

                t_paquete* paquete5 = crear_paquete(MENSAJE_MASTER_QUERY, buffer5);
                enviar_paquete(paquete5, query5->fd_queryControl);
                eliminar_paquete(paquete5);
            break;
            case(FIN_POR_ERROR):
                t_buffer* buffer7 = recibir_todo_un_buffer(worker->fd_worker);
                int query_id7 = extraer_int_del_buffer(buffer7);
                char* motivo7 = extraer_string_del_buffer(buffer7);
                char* motivo = strdup(motivo7);
                destruir_buffer(buffer7);
                query_t* query7 = buscar_query_con_id_en(query_id7,EXEC);

                pasar_query_de_estado_a_otro(query7,EXEC,EXIT);

                pthread_mutex_lock(&mutex_lista_workers);
                worker->estaLibre=true;
                //worker->query_tomada=-1;
                pthread_mutex_unlock(&mutex_lista_workers);
                
                t_buffer* buffer_fin = crear_buffer();
                cargar_string_al_buffer(buffer_fin,motivo);

                t_paquete* paquete7 = crear_paquete(FIN_MASTER_QUERY,buffer_fin);
                enviar_paquete(paquete7, query7->fd_queryControl);
                eliminar_paquete(paquete7);

                sem_post(&sem_hay_workes_disponibles);
            break;
            default:
                log_info(master_logger, "operacion incorrecta de worker");
            break;
        }
    }
    return NULL;
}

void* espera_de_rta_de_queryControl(void* queryy){
    query_t* query = (query_t*) queryy;
    int i=0;
    while(i==0){
        int codigo = recibir_operacion(query->fd_queryControl);
        switch(codigo){
            case(-1):
                int cantidad = calcular_nivel_multiprogramacion();
                log_info(master_logger, "## Se desconecta un Query Control. Se finaliza la Query <%d> con prioridad <%d>. Nivel multiprogramación <%d>", query->id, query->prioridad, cantidad);
                sacar_query_de_algun_estado(query);
                liberar_query(query);
                i=1;
            break;
            case(CONFIRMACION_DE_FINALIZACION_DE_QUERY):
                //log_debug(master_logger, "termino el hilo de esperar_rta  de query %d", query->id);
                t_buffer* buffer =recibir_todo_un_buffer(query->fd_queryControl);
                eliminar_buffer(buffer);
                destruir_queries(query);
                i=1;
            break;
            default:
                log_info(master_logger, "operacion incorrecta de query");
            break;
        }
    }
    return NULL;

}
