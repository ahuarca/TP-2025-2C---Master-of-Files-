#include "./include/queryControl.h"
#include <commons/string.h>

int main(int argc, char* argv[]) {


    if(argc<4){
        perror("No se indicaron los parametros suficientes. Finalizando ejecucion...");
        return 1;
    }

    cargar_argumentos(argv);
    iniciar_QueryControl();
    establecer_conexion();
    gestionar_master();

    return 0;
    
}

void cargar_argumentos(char* argv[]){
    archivo_config = argv[1];
    archivo_query = argv[2];
    prioridad = atoi(argv[3]);
}

void iniciar_QueryControl(){
    queryControl_config = iniciar_config(archivo_config);
    cargar_config();
    queryControl_logger = iniciar_logger("queryControl.log","QUERY_CONTROL",LOG_LEVEL);

}

void cargar_config(){
    IP_MASTER     = config_get_string_value(queryControl_config,"IP_MASTER");
    PUERTO_MASTER = config_get_string_value(queryControl_config,"PUERTO_MASTER");
    LOG_LEVEL     = config_get_string_value(queryControl_config,"LOG_LEVEL");
}

void establecer_conexion(){
    fd_conexion = crear_conexion(IP_MASTER,PUERTO_MASTER);
    log_info(queryControl_logger,"## Conexion al Master exitosa. IP: <%s>, Puerto: <%s>",IP_MASTER,PUERTO_MASTER);
    enviar_handshake();
    log_info(queryControl_logger,"## Solicitud de ejecucion de Query: <%s>, prioridad: <%d>",archivo_query,prioridad);
    
}

void enviar_handshake(){
    t_buffer* buffer = crear_buffer();
    cargar_string_al_buffer(buffer,archivo_query);
    cargar_entero_al_buffer(buffer,prioridad);
    t_paquete* paquete = crear_paquete(HANDSHAKE_QUERY_MASTER,buffer);
    enviar_paquete(paquete, fd_conexion);
    eliminar_paquete(paquete);
}

void* gestionar_master(){
    while(1){
        int codigo = recibir_operacion(fd_conexion);

        switch (codigo)
        {
        case MENSAJE_MASTER_QUERY:
            t_buffer* buffer = recibir_todo_un_buffer(fd_conexion);
            char* nombre_file = extraer_string_del_buffer(buffer);
            char* mensaje = extraer_string_del_buffer(buffer);
            destruir_buffer(buffer);
            log_info(queryControl_logger,"## Lectura realizada: Archivo <%s>, contenido: <%s>",nombre_file,mensaje);
            break;
        case FIN_MASTER_QUERY:
            t_buffer* buffer_fin = recibir_todo_un_buffer(fd_conexion);
            char* motivo = extraer_string_del_buffer(buffer_fin);
            destruir_buffer(buffer_fin);
            log_info(queryControl_logger,"## Query finalizada - <%s>",motivo);

            //envio la confimacion de la finalizacion
            t_buffer* buffer_confirmacion = crear_buffer();
            cargar_entero_al_buffer(buffer_confirmacion,1);
            t_paquete* paquete = crear_paquete(CONFIRMACION_DE_FINALIZACION_DE_QUERY,buffer_confirmacion);
            enviar_paquete(paquete, fd_conexion);
            eliminar_paquete(paquete);
            return 0;
        case -1:
            log_error(queryControl_logger,"Ocurrio un suceso inesperado, finalizando ejecucion...");
            return 0;
        default:
            break;
        }
    }
}