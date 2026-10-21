#include "main.h"
#include <commons/collections/dictionary.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <filesystem/util.h>
#include <utils/conexiones.h>
#include <utils/utils.h>
#include <signal.h>
#include "inicializacion/inicializacion.h"
#include "config/config_reader.h"
#include "./adapter/operaciones.h"
#include <sys/mman.h>

t_config* config_hash;
t_custom_config* configuration;
t_config_super_block* super_config;
t_log* logger;
//socket server
int fd_server;
// Diccionarios
t_dictionary* dict_files;
//listas
t_list* lista_bloques_fisicos;
t_list* lista_workers;
//variables globales
int TERMINAR = 0;
int CANT_WORKERS_CONECTADOS = 0;
int CANT_BLOQUES = 0;
// Bitmap bloques
void* archivo_bitmap_bloques_mapeado;
t_bitarray* bitarray_bloques;
//archivos
FILE* archivo_block_hash_index;
//mutex
pthread_mutex_t mutexCantWorkers;
pthread_mutex_t mutexConfigHash;
pthread_mutex_t mutexListaBloquesFisicos;
pthread_mutex_t mutexDirectorioFiles;
pthread_mutex_t mutexBitarrayBloques;

static void iniciar_storage(char* config_nombre);
static void* manejar_cliente_worker(void* arg);
static void iniciar_listas_y_diccionarios();
static void iniciar_semaforos();
static t_custom_config* load_config_helper(char* config_path);
static char* obtener_motivo(op_code codigo);
static void liberar_instruccion(t_worker_instruccion* instruccion);
static t_worker_instruccion* inicializar_instruccion();
static void manejar_sigint(int senial);
static void liberar_recursos();
static void cerrar_conexiones_con_workers();
static void liberar_estructuras();
static void  destruir_diccionarios();
static void  destruir_file(void* ptr);
static void destruir_tag(void* ptr);
static void destruirBloquesTag(void* ptr);
static void destruir_lista_bloques_fisicos();
static void destruir_bloque_fisico(void* ptr);
static int free_bitmap_data(void* data, size_t size);


int main(int argc, char* argv[])
{
    
    signal(SIGINT, manejar_sigint);

    if(argc < 2){
        printf("Error en la cantidad de parametros\n");
        return -1;
    }

    iniciar_listas_y_diccionarios();
    iniciar_semaforos();
    iniciar_storage(argv[1]);
    fd_server = iniciar_servidor(configuration->puerto_escucha, logger, "STORAGE iniciado");
    log_info(logger, "Iniciando servidor...");
    //esto va a ser una servidor multihilo esperando las conexiones de los 3 workers
    while (!TERMINAR)
    {
        int* fd_conexion = malloc(sizeof(int));
        *fd_conexion = esperar_cliente(fd_server);

        log_info(logger, "se conecto un worker");
        pthread_t hilo_cliente;
        pthread_create(&hilo_cliente, NULL, manejar_cliente_worker, (void*)fd_conexion);
        pthread_detach(hilo_cliente);
    }
    return 0;
}

void iniciar_storage(char* config_nombre){
    char path[50] = "config/";
    strcat(path,config_nombre);
    printf("\n%s\n", path);
    configuration = load_config_helper(path);
   
    logger = iniciar_logger("storage.log", "STORAGE_LOG", configuration->log_level);
    log_debug(logger, "%s", configuration->fresh_start);
    log_info(logger, "BLOCK SIZE = %d , TAM_FS = %d ", configuration->initial_block_size,configuration->initial_fs_size);

    if (strcmp(configuration->fresh_start, "TRUE") == 0)
    {
        log_info(logger, "Iniciando con fresh start");
        if (inicializar_storage() < 0)
        {
            log_error(logger, "Error inicializacion");
            exit(EXIT_FAILURE);
        }

        commit_tag("initial_file","BASE",-1);
    }
    else
    {
        super_config = malloc(sizeof(t_config_super_block));
        super_config->block_size = configuration->initial_block_size;
        super_config->fs_size = configuration->initial_fs_size;

        CANT_BLOQUES =  super_config->fs_size / super_config->block_size;

        for(int i=0; i<CANT_BLOQUES;i++){
            int rta = crear_struct_para_t_bloque_fisico(i);
                if(rta < 0){
                    log_error(logger, "fallo al crear path o en el malloc del t_bloque_fisico");
                    exit(1);
               }
        }


        rellenar_diccionarios("/home/utnso/storage/files");
       
        int bitRta = crear_bitarray_para_fresh_false();
        if(bitRta < 0){
            log_error(logger, "fallo al crear en el fresh false, el bitarray");
            exit(1);
        }

        int configRta = crear_config_hash_para_fresh_false();
            if(configRta < 0){
                log_error(logger, "fallo al crear en el fresh false en config hash");
                exit(1);
            }
        log_info(logger, "Iniciando SIN fresh start");
}
}

void iniciar_listas_y_diccionarios()
{
    dict_files = dictionary_create();
    lista_bloques_fisicos = list_create();
    lista_workers = list_create();
}
void iniciar_semaforos(){
    pthread_mutex_init(&mutexCantWorkers, NULL);
    pthread_mutex_init(&mutexConfigHash, NULL);
    pthread_mutex_init(&mutexListaBloquesFisicos, NULL);
    pthread_mutex_init(&mutexDirectorioFiles, NULL);
    pthread_mutex_init(&mutexBitarrayBloques, NULL);
}

void* manejar_cliente_worker(void* arg)
{
    int id_worker = -1;
    const int fd_conexion = *(int*)arg;
    free(arg);

    while (!TERMINAR)
    {
        const int cod_op = recibir_operacion(fd_conexion);

        switch (cod_op)
        {
        case HANDSHAKE_WORKER_STORAGE:
            {
                log_info(logger, "Se recibió el handshake del worker");
                t_buffer* buffer_hanshake = recibir_todo_un_buffer(fd_conexion);
                id_worker = extraer_int_del_buffer(buffer_hanshake);
                destruir_buffer(buffer_hanshake);
                //Respuesta
                t_buffer* buffer_datos = crear_buffer();
                cargar_entero_al_buffer(buffer_datos, super_config->block_size);
                t_paquete* paquete = crear_paquete(DATO_MEMORIA, buffer_datos);
                enviar_paquete(paquete, fd_conexion);
                eliminar_paquete(paquete);
                // Verifico que el worker se conectó correctamente
                if (id_worker == -1)
                {
                    log_error(logger, "Se recibio un id invalido del worker");
                    TERMINAR = 1;
                    break;
                }
                pthread_mutex_lock(&mutexCantWorkers);
                CANT_WORKERS_CONECTADOS++;
                pthread_mutex_unlock(&mutexCantWorkers);
                int* fd_worker = malloc(sizeof(int));
                *fd_worker = fd_conexion;
                list_add(lista_workers,fd_worker);
                //log obligatorio
                log_info(
                    logger,
                    "Conexión de Worker:##Se conecta el Worker <%d> - Cantidad de Workers: <%d>",
                    id_worker, CANT_WORKERS_CONECTADOS
                );
            }
            break;
        case SOL_CREATE:
            {
                usleep(configuration->retardo_operacion*1000);
                log_info(logger, "Se recibió el SOL_CREATE del worker");
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                int pid_create = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                int rta = create_file(inst->nombre_file, inst->tag,pid_create);

                if(rta < 0){
                    if(rta == -1){
                        char* motivo = obtener_motivo(FILE_PREEXISTENTE);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(FILE_PREEXISTENTE,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        log_warning(logger, "El %s:%s no se puede crear porque ya existe",inst->nombre_file,inst->tag);
                        liberar_instruccion(inst);
                        break;
                
                    }else{
                        char* motivo = obtener_motivo(FIN_POR_ERROR_STORAGE);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(FIN_POR_ERROR_STORAGE,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                        
                    }
                }
               
                log_info(logger, "File Creado: ##<QUERY_ID %d> - File Creado <%s>:<%s>",pid_create, inst->nombre_file, inst->tag);
                enviar_handshake_especial(RTA_CREATE,fd_conexion);
                liberar_instruccion(inst);
                break;
            }
       
        case SOL_TRUNCATE:
            {
                usleep(configuration->retardo_operacion*1000);
                log_info(logger, "Se recibió el SOL_TRUNCATE del worker");
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                inst->primer_parametro_entero = extraer_int_del_buffer(buff);
                int pid_truncate = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                int rta = truncar_file(inst->nombre_file, inst->tag, inst->primer_parametro_entero,pid_truncate);
                    if(rta == -10 || rta == -3){
                        log_warning(logger, "El tag %s no pudo ser truncado porque ya no quedan bloques libres y no reservo ni un bloque fisico", inst->tag);
                         char* motivo = obtener_motivo(ESPACIO_INSUFICIENTE);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(ESPACIO_INSUFICIENTE,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                    if(rta == -1){
                        log_warning(logger, "El tag %s no pudo ser truncado porque no existe el file o el tag", inst->tag);
                        char* motivo = obtener_motivo(FILE_INEXISTENTE);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(FILE_INEXISTENTE,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                    if(rta == -9){
                        log_warning(logger, "El tag %s no pudo ser truncado porque esta commiteado", inst->tag);
                        char* motivo = obtener_motivo(ESCRITURA_NO_PERMITIDA);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(ESCRITURA_NO_PERMITIDA,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                //log obligatorio
                log_info(logger,"##<QUERY_ID %d> - File Truncado <%s>:<%s> - Tamaño: <%d>",pid_truncate,inst->nombre_file, inst->tag, inst->primer_parametro_entero);
                enviar_handshake_especial(RTA_TRUNCATE,fd_conexion);
                liberar_instruccion(inst);

            }
            break;
        case SOL_READ:
            {
                usleep(configuration->retardo_operacion*1000);
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                inst->primer_parametro_entero = extraer_int_del_buffer(buff);//numero de pagina = numero de bloque logico
                int pid_read = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                log_info(logger, "Se recibió el SOL_READ del worker PID %d, FILE:TAG %s:%s",pid_read,inst->nombre_file,inst->tag);
                char* content = NULL;
                const int error = leer_bloque(&content, inst->nombre_file, inst->tag, inst->primer_parametro_entero);

                if (error < 0)
                {
                    if(error == -1){
                        log_warning(logger, "El tag %s no pudo ser copiado porque el file o tag  %s no existe", inst->tag, inst->primer_parametro_cadena);
                        
                        char* motivo = obtener_motivo(FILE_INEXISTENTE);
                        t_buffer* buffer = crear_buffer();
                        cargar_string_al_buffer(buffer, motivo);
                        t_paquete* paquete = crear_paquete(FILE_INEXISTENTE,buffer);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                    if(error == -9){
                    // Retornar error interno
                    char* motivo = obtener_motivo(LECTURA_FUERA_DE_LIMITE);
                    t_buffer* buffer = crear_buffer();
                    cargar_string_al_buffer(buffer, motivo);
                    t_paquete* paquete = crear_paquete(LECTURA_FUERA_DE_LIMITE,buffer);
                    enviar_paquete(paquete, fd_conexion); 
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                    }
                    else{
                    char* motivo = obtener_motivo(ERROR_STORAGE);
                    t_buffer* buffer = crear_buffer();
                    cargar_string_al_buffer(buffer, motivo);
                    t_paquete* paquete = crear_paquete(ERROR_STORAGE,buffer);
                    enviar_paquete(paquete, fd_conexion); 
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                    }
                   
                }
                // Respuesta Ok
                t_buffer* respuesta = crear_buffer();
                cargar_string_al_buffer(respuesta,content);
                t_paquete* paquete_read = crear_paquete(RTA_READ,respuesta);
                enviar_paquete(paquete_read,fd_conexion);
                eliminar_paquete(paquete_read);
                // enviar_handshake_especial(RTA_READ, fd_conexion); //worker solo espera el codigo de operacion


                //log obligatorio
                log_info(logger, "##<QUERY_ID %d> - Bloque Lógico Leído <%s>:<%s> - Número de Bloque: <%d>",pid_read, inst->nombre_file,inst->tag, inst->primer_parametro_entero);
                liberar_instruccion(inst);
            }

            break;
        case SOL_WRITE:
            {
                usleep(configuration->retardo_operacion*1000);
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                inst->primer_parametro_entero = extraer_int_del_buffer(buff);
                inst->primer_parametro_cadena = extraer_string_del_buffer(buff);
                int pid = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                log_info(logger, "Se recibió el SOL_WRITE del worker PID %d, FILE:TAG %s:%s",pid, inst->nombre_file,inst->tag);
                const int error = escribir_bloque(inst->nombre_file, inst->tag, inst->primer_parametro_entero, inst->primer_parametro_cadena, pid);
                if (error < 0)
                {
                    if(error == -1){
                        // Retornar error interno
                        t_buffer* buffer_datos = crear_buffer();
                        char* motivo = obtener_motivo(FILE_INEXISTENTE);
                        cargar_string_al_buffer( buffer_datos ,motivo);
                        t_paquete* paquete = crear_paquete(FILE_INEXISTENTE, buffer_datos);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }

                    else if(error == -3){
                    // Retornar error interno
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(ESCRITURA_NO_PERMITIDA);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(ESCRITURA_NO_PERMITIDA, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                    }
                    else if(error == -5){
                    // Retornar error interno
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(ESCRITURA_FUERA_DE_LIMITE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(ESCRITURA_FUERA_DE_LIMITE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                    }
                    else if(error == -2){
                        // Retornar error interno
                        t_buffer* buffer_datos = crear_buffer();
                        char* motivo = obtener_motivo(ESPACIO_INSUFICIENTE);
                        cargar_string_al_buffer( buffer_datos ,motivo);
                        t_paquete* paquete = crear_paquete(ESPACIO_INSUFICIENTE, buffer_datos);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                    else{
                        t_buffer* buffer_datos = crear_buffer();
                        char* motivo = obtener_motivo(ERROR_STORAGE);
                        cargar_string_al_buffer( buffer_datos ,motivo);
                        t_paquete* paquete = crear_paquete(ERROR_STORAGE, buffer_datos);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                    }
                   
                }
               
                // Respuesta Ok
                t_buffer* buffer_datos = crear_buffer();
                cargar_string_al_buffer(buffer_datos, "listo");
                t_paquete* paquete = crear_paquete(RTA_READ, buffer_datos);
                enviar_paquete(paquete, fd_conexion);
                eliminar_paquete(paquete);
                log_info(logger, "Bloque Lógico Escrito: ##<QUERY_ID> <%d>- Bloque Lógico Escrito <%s>:<%s> - Número de Bloque: <%d>",pid , inst->nombre_file, inst->tag, inst->primer_parametro_entero);
                liberar_instruccion(inst);
                break;
            } 
            break;
        case SOL_TAG:
            {
                usleep(configuration->retardo_operacion*1000);
              
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);// nombre del file origen
                inst->tag = extraer_string_del_buffer(buff);// nombre del tag origen
                inst->primer_parametro_cadena = extraer_string_del_buffer(buff);// nombre del file destino del tag
                inst->segundo_parametro_cadena = extraer_string_del_buffer(buff);//nombre nuevo del tag
                int pid = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                log_info(logger, "Se recibió el SOL_TAG del worker PID %d, FILE:TAG %s:%s",pid,inst->nombre_file,inst->tag);
                int rta = copiar_tag(inst->nombre_file, inst->tag, inst->primer_parametro_cadena, inst->segundo_parametro_cadena, pid);

                if(rta == -1){
                        log_warning(logger, "El tag %s no pudo ser copiado porque el file o tag  %s no existe", inst->tag, inst->segundo_parametro_cadena);
                        t_buffer* buffer_datos = crear_buffer();
                        char* motivo = obtener_motivo(FILE_INEXISTENTE);
                        cargar_string_al_buffer( buffer_datos ,motivo);
                        t_paquete* paquete = crear_paquete(FILE_INEXISTENTE, buffer_datos);
                        enviar_paquete(paquete, fd_conexion);
                        eliminar_paquete(paquete);
                        liberar_instruccion(inst);
                        break;
                }
                if(rta == -2 ){
                    log_warning(logger, "El tag %s no pudo ser copiado porque hubo un error al crear la estrutura del nuevo tag", inst->tag);
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(ERROR_STORAGE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(ERROR_STORAGE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                    
                }
                if(rta == -3){
                    log_warning(logger, "El tag %s no pudo ser copiado porque hubo un error raro", inst->tag);
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(ERROR_STORAGE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(ERROR_STORAGE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                }
                if(rta== -9){
                    log_warning(logger, "El tag no pudo ser copiado porque ya existe el file:tag %s %s",inst->nombre_file ,inst->tag);
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(TAG_PREEXISTENTE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(TAG_PREEXISTENTE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                }
                liberar_instruccion(inst);
                enviar_handshake_especial(RTA_TAG,fd_conexion);
            }
            break;
        case SOL_TAG_COMMIT:
            {
                usleep(configuration->retardo_operacion*1000);
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst = inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                int pid_commit = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                log_info(logger, "Se recibió el SOL_TAG _COMMIT del worker PID %d, FILE:TAG %s:%s",pid_commit,inst->nombre_file,inst->tag);
                int rta = commit_tag(inst->nombre_file, inst->tag, pid_commit);
                if(rta == -1){
                    log_warning(logger, "El tag no pudo ser comiteado porque el file: tag %s:%s no existe", inst->nombre_file,inst->tag);
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(FILE_INEXISTENTE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(FILE_INEXISTENTE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                }
                if(rta == -5){
                    log_warning(logger, "El file :tag %s %s no pudo ser commiteado porque ya estaba commiteado",   inst->nombre_file ,inst->tag);
                    enviar_handshake_especial(RTA_COMMIT,fd_conexion);
                    liberar_instruccion(inst);
                    break;
                }
                if(rta == -6 || rta == -4){
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(ERROR_STORAGE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(ERROR_STORAGE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                }
                log_info(logger, "##<QUERY_ID><%d> - Commit de File:Tag <%s>:<%s>",pid_commit,inst->nombre_file, inst->tag);
                enviar_handshake_especial(RTA_COMMIT,fd_conexion);
                liberar_instruccion(inst);
            }
            break;
        case SOL_TAG_DELETE:
            {
                usleep(configuration->retardo_operacion*1000);
                log_info(logger, "Se recibió el SOL_TAG_DELETE del worker");
                t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
                t_worker_instruccion* inst =inicializar_instruccion();
                inst->nombre_file = extraer_string_del_buffer(buff);
                inst->tag = extraer_string_del_buffer(buff);
                int pid_delete = extraer_int_del_buffer(buff);
                destruir_buffer(buff);
                int rta = delete_tag(inst->nombre_file, inst->tag, pid_delete);
                if(rta < 0){
                    log_warning(logger, "El tag no pudo ser eliminado porque el file:tag  %s no existe",  inst->nombre_file,inst->tag);
                    t_buffer* buffer_datos = crear_buffer();
                    char* motivo = obtener_motivo(FILE_INEXISTENTE);
                    cargar_string_al_buffer( buffer_datos ,motivo);
                    t_paquete* paquete = crear_paquete(FILE_INEXISTENTE, buffer_datos);
                    enviar_paquete(paquete, fd_conexion);
                    eliminar_paquete(paquete);
                    liberar_instruccion(inst);
                    break;
                }
                log_info(logger, "##<QUERY_ID><%d> - Tag Eliminado <%s>:<%s>",pid_delete, inst->nombre_file, inst->tag);
                enviar_handshake_especial(RTA_DELETE,fd_conexion);
                liberar_instruccion(inst);
            }
            break;
        case SOL_FLUSH:
        {
            usleep(configuration->retardo_operacion*1000);
            log_info(logger, "Se recibió el SOL_FLUSH del worker");
            t_buffer* buff = recibir_todo_un_buffer(fd_conexion);
            t_worker_instruccion* inst =inicializar_instruccion();
            inst->nombre_file = extraer_string_del_buffer(buff);
            inst->tag = extraer_string_del_buffer(buff);
            char* contenido_a_persistir = extraer_string_del_buffer(buff);
            int numero_bloque_logico = extraer_int_del_buffer(buff);
            destruir_buffer(buff);

            flush_file(inst->nombre_file, inst->tag, contenido_a_persistir,numero_bloque_logico);
            log_info(logger, "Flush de File:Tag: ##<%s>:<%s>",inst->nombre_file, inst->tag);
            enviar_handshake_especial(RTA_FLUSH,fd_conexion);   
            liberar_instruccion(inst);      
            break;
          
        }
        case -1:
            if (id_worker != -1)
            {   
                pthread_mutex_lock(&mutexCantWorkers);
                CANT_WORKERS_CONECTADOS--;
                pthread_mutex_unlock(&mutexCantWorkers);

                //log obligatorio
                log_info(
                    logger,
                    "Desconexión de Worker: ##Se desconecta el Worker <%d> - Cantidad de Workers: <%d>",
                    id_worker, CANT_WORKERS_CONECTADOS
                );
            }
            close(fd_conexion);
            return NULL; 
            break;
        default:
            log_warning(logger, "La operacion desconocida es esta %d ", cod_op);
            break;
        }
    }

    return NULL;
}


t_custom_config* load_config_helper(char* config_path)
{
    t_custom_config* cfg = malloc(sizeof(t_custom_config));
    int error = read_config(cfg, config_path);
    if (error != 0)
    {
        if (error == -2)
        {
            printf("No existe el archivo config en el path %s  ", config_path);
        }
        free(cfg);
        exit(EXIT_FAILURE);
    }

    return cfg;
}

char* obtener_motivo(op_code codigo){


    switch (codigo)
    {
    case FILE_INEXISTENTE:
        return "File inexistente";
        break;
    case TAG_PREEXISTENTE:
        return "Tag preexistente";
    case FILE_PREEXISTENTE:
         return "File preexistente";
        break;
    case ESPACIO_INSUFICIENTE:
        return "Espacio Insuficiente";
        break;
    case ESCRITURA_NO_PERMITIDA:
        return "Escritura no permitida";
        break;
    case LECTURA_FUERA_DE_LIMITE:
        return "Lectura fuera de limite";
        break;
    case ESCRITURA_FUERA_DE_LIMITE:
        return "Escritura fuera de limite";
        break;
    case ERROR_STORAGE:
        return "Error storage";
        break;
    case FIN_POR_ERROR_STORAGE:
        return "FIN_POR_ERROR_STORAGE";
        break;
    default:
         return "motivo desconocido";
        break;
    }
}



void liberar_instruccion(t_worker_instruccion* instruccion){

    if(instruccion!=NULL){
        if(instruccion->operacion!=NULL){
                free(instruccion->operacion);
        }

        if(instruccion->nombre_file!=NULL){
            free(instruccion->nombre_file);
        }
        
        if(instruccion->tag!=NULL){
            free(instruccion->tag);
        }

        if(instruccion->primer_parametro_cadena!=NULL){
            free(instruccion->primer_parametro_cadena);
        }

        if(instruccion->segundo_parametro_cadena!=NULL){
            free(instruccion->segundo_parametro_cadena);
        }

        free(instruccion);
    }


}


static t_worker_instruccion* inicializar_instruccion(){

    t_worker_instruccion* inst = malloc(sizeof(t_worker_instruccion));
    inst->nombre_file = NULL;
    inst->operacion = NULL;
    inst->primer_parametro_cadena = NULL;
    inst->segundo_parametro_cadena = NULL;
    inst->tag = NULL;

    return inst;

}


void manejar_sigint(int senial){
    printf("\n Señal SIGINT recibida. Terminando...\n");
    TERMINAR = 1;
    liberar_recursos();
    exit(0); 
}

void liberar_recursos(){

    printf("\nLiberando recursos...\n");

    if (fd_server != -1) {
        close(fd_server);
        printf("Socket de escucha Storage cerrado.\n");
    }

    

    // cerrar_conexiones_con_workers();
    // printf("lleago hasta antes de liberar estructuras");
    // liberar_estructuras();
    // printf("lleago hasta antes de liberar detsruir diccionarios");
    // destruir_diccionarios();
    // printf("lleago hasta antes de detsuir b f");
    // destruir_lista_bloques_fisicos();
    // free(super_config);


}

void cerrar_conexiones_con_workers(){
    int tam = list_size(lista_workers);

    printf("tam %d \n", tam);

    for (int i = 0; i < tam; i++) {
        int* fd = list_get(lista_workers, i);

        if (fd != NULL && (*fd)!=-1) {
            close(*fd);   
            printf("Socket de conexión con worker cerrado.\n");
            
        }  
        
    }
   
    if(lista_workers != NULL){
        list_destroy_and_destroy_elements(lista_workers,destruirBloquesTag);
    }
   
}

void liberar_estructuras(){
 
    config_destroy(config_hash);
    free(configuration->fresh_start);
    free(configuration->log_level);
    free(configuration->punto_montaje);
    config_destroy(configuration->config_bk);
    destroy_logger(logger);
    free_bitmap_data(archivo_bitmap_bloques_mapeado, CANT_BLOQUES);
    close(fd_bitmap);
    bitarray_destroy(bitarray_bloques);

}

void destruir_diccionarios(){
    if(dict_files!=NULL && !dictionary_is_empty(dict_files)){
        dictionary_destroy_and_destroy_elements(dict_files, destruir_file);
    }else{
        dictionary_destroy(dict_files);
    }
  
   	   
}

void destruir_file(void* ptr){
    t_file* file = (t_file*) ptr;
    free(file->name);
    free(file->path);
    dictionary_destroy_and_destroy_elements(file->dict_tags,destruir_tag);	
    free(file);
}

void destruir_tag(void* ptr){
    t_tag* tag = (t_tag*) ptr;
    free(tag->nombre_tag);
    config_destroy(tag->metadata_config);
    list_destroy_and_destroy_elements(tag->lista_bloques, destruirBloquesTag);
    free(tag);
}

void destruirBloquesTag(void* ptr){
    int* num_bloque = (int*)ptr;
    free(num_bloque);
}

void destruir_lista_bloques_fisicos(){
    list_destroy_and_destroy_elements(lista_bloques_fisicos,destruir_bloque_fisico);
    
}

void destruir_bloque_fisico(void* ptr){
    t_bloque_fisico* bloque = (t_bloque_fisico*) ptr;
	free(bloque->nombre_bloque_fisico);
    munmap(bloque->mmap_ptr,super_config->block_size);
    free(bloque->ruta_del_bloque_fisico);
	free(bloque);
}

// esta función se utilizara más adelante para la liberación de memoria del modulo
int free_bitmap_data(void* data, size_t size){
    if (munmap(data, size) == -1) {
        perror("munmap");
        return -1;
    }
    return 0;
}


