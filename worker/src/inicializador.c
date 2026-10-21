#include "./include/inicializador.h"

void iniciar_worker(){

    cargar_config();
    crear_logger();
    conectar_con_storage();
    conectar_con_master();
};

void cargar_argumentos(char* argv[]){
    archivo_config = argv[1];
    id_worker = atoi(argv[2]);
}

void crear_logger(){

    char archivo_logger[30];
    char nombre_logger[30];
    sprintf(archivo_logger,"worker_%d.log",id_worker);
    sprintf(nombre_logger,"WORKER_%d_LOG",id_worker);
    worker_logger = iniciar_logger(archivo_logger,nombre_logger,LOG_LEVEL);
    log_info(worker_logger,"Worker <%d> iniciado", id_worker);
}

void cargar_config(){

    worker_config = iniciar_config(archivo_config);

    IP_MASTER               = config_get_string_value(worker_config,"IP_MASTER");
    PUERTO_MASTER           = config_get_string_value(worker_config,"PUERTO_MASTER");
    PUERTO_MASTER_INTERRUPT = config_get_string_value(worker_config,"PUERTO_MASTER_INTERRUPT");
    IP_STORAGE              = config_get_string_value(worker_config,"IP_STORAGE");
    PUERTO_STORAGE          = config_get_string_value(worker_config,"PUERTO_STORAGE");
    TAM_MEMORIA             = config_get_int_value(worker_config,"TAM_MEMORIA");
    RETARDO_MEMORIA         = config_get_int_value(worker_config,"RETARDO_MEMORIA");
    ALGORITMO_REEMPLAZO     = config_get_string_value(worker_config,"ALGORITMO_REEMPLAZO");
    PATH_SCRIPTS            = config_get_string_value(worker_config,"PATH_SCRIPTS");
    LOG_LEVEL               = config_get_string_value(worker_config,"LOG_LEVEL");

}


void conectar_con_storage(){

    fd_storage = crear_conexion(IP_STORAGE,PUERTO_STORAGE);

    t_buffer* buffer = crear_buffer();
    cargar_entero_al_buffer(buffer,id_worker);
    t_paquete* paquete = crear_paquete(HANDSHAKE_WORKER_STORAGE,buffer);
    enviar_paquete(paquete,fd_storage);

    eliminar_paquete(paquete);

    int code_op = recibir_operacion(fd_storage);

    if(code_op == DATO_MEMORIA){

        t_buffer* buffer = recibir_todo_un_buffer(fd_storage);

        TAM_PAGINA = extraer_int_del_buffer(buffer);

        eliminar_buffer(buffer);

    }
}

void conectar_con_master(){

    fd_master = crear_conexion(IP_MASTER,PUERTO_MASTER);
    t_buffer* buffer = crear_buffer();
    cargar_entero_al_buffer(buffer,id_worker);

    t_paquete* paquete = crear_paquete(HANDSHAKE_WORKER_MASTER,buffer);
    enviar_paquete(paquete,fd_master);

    eliminar_paquete(paquete);

    // Conexion al puerto interrupt

    fd_master_interrupt = crear_conexion(IP_MASTER,PUERTO_MASTER_INTERRUPT);
    
    t_buffer* buffer_interrupt = crear_buffer();
    cargar_entero_al_buffer(buffer_interrupt,id_worker);
    
    t_paquete* paquete_interrupt = crear_paquete(HANDSHAKE_WORKER_MASTER_INTERRUPT,buffer_interrupt);
    enviar_paquete(paquete_interrupt,fd_master_interrupt);

    eliminar_paquete(paquete_interrupt);
}