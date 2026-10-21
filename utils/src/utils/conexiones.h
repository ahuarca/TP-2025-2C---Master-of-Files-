#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <netdb.h>
#include <commons/log.h>
#include <commons/config.h>


typedef enum
{
	PAQUETE,
	FIN_MASTER_QUERY_I,
	FIN_MASTER_QUERY_C,

	// Handshake_Envia_Recibe

	HANDSHAKE_QUERY_MASTER,
	HANDSHAKE_WORKER_STORAGE,
	HANDSHAKE_WORKER_MASTER,
	HANDSHAKE_WORKER_MASTER_INTERRUPT,

	// Mensajes de Master a Query
	CONFIRMACION_DE_FINALIZACION_DE_QUERY, 
	MENSAJE_MASTER_QUERY,
	FIN_MASTER_QUERY,

	// Mensajes de Master a Worker

	EJECUTAR_QUERY,
	DESALOJAR_QUERY,
	DESALOJAR_QUERY_DESCONEXION,

	// Mensajes de Storage a Worker

	DATO_MEMORIA,
	RTA_CREATE,
	RTA_TRUNCATE,
	RTA_WRITE,
	RTA_READ,
	RTA_TAG,
	RTA_COMMIT,
	RTA_FLUSH,
	RTA_DELETE,

	// Mensajes de Worker a Storage

	SOL_CREATE,
	SOL_TRUNCATE,
	SOL_TAG,
	SOL_TAG_COMMIT,
	SOL_TAG_DELETE,
	SOL_READ,
	SOL_WRITE,
	SOL_FLUSH,
	SOL_CONTENIDO_PAGINA,

	// Mensajes de Worker a Master

	RTA_DESALOJO,
	FIN_POR_ERROR,
	LECTURA_WORKER_MASTER,
	SOL_END,
	RTA_DESALOJO_DESCONEXION,

	// Errores de Storage para Worker

	FILE_INEXISTENTE,
	FILE_PREEXISTENTE,
	ESPACIO_INSUFICIENTE,
	ESCRITURA_NO_PERMITIDA,
	LECTURA_FUERA_DE_LIMITE,
	ESCRITURA_FUERA_DE_LIMITE,
	TAG_PREEXISTENTE,
	ERROR_STORAGE,
	FIN_POR_ERROR_STORAGE

}op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


//FUNCIONES INICIALIZADORAS DE CONFIG Y LOGGERS
t_log* iniciar_logger(char* direccion, char* infConetenida, char* log_lvl);
t_config* iniciar_config(char* direccion);

int crear_conexion(char *ip, char* puerto);
int iniciar_servidor(int puerto, t_log* logger, char* mensaje);
int esperar_cliente(int socket_servidor);
int recibir_operacion(int socket_cliente);

//FUNCIONES COMPARTIDAS ENTRE MODULOS
void enviar_hand_shake_cpu(op_code operacion, int fd_server, int identificador);
void enviar_hand_shake_kernel_mem(op_code operacion, int fd_server);


// Funciones cargar a un buffer

void cargar_choclo_al_buffer(t_buffer* un_buffer,void* choclo, int size_choclo);
void cargar_entero_al_buffer(t_buffer* un_buffer, int value);
void cargar_string_al_buffer(t_buffer* un_buffer, char* un_string);
void cargar_uint32_al_buffer(t_buffer* un_buffer,uint32_t value);
void cargar_double_al_buffer(t_buffer* un_buffer, double value);

t_buffer* recibir_todo_un_buffer(int fd);
t_buffer* crear_buffer();
void eliminar_buffer(t_buffer *buffer);
void destruir_buffer(t_buffer* un_buffer);
t_paquete* crear_paquete(op_code codigo, t_buffer* un_buffer);
void* serializar_paquete(t_paquete* un_paquete);
void enviar_handshake_especial(op_code codigo, int conexion);
void enviar_paquete(t_paquete* un_paquete, int conexion);
void eliminar_paquete(t_paquete* paquete);
void eliminar_paquete(t_paquete *paquete);
t_paquete *iniciar_paquete(int cod_op);


void* extraer_choclo_del_buffer(t_buffer* un_buffer);
int extraer_int_del_buffer(t_buffer* buffer);
uint32_t extraer_uint32_del_buffer(t_buffer* buffer);
double extraer_double_del_buffer(t_buffer* un_buffer);
char* extraer_string_del_buffer(t_buffer* buffer);


#endif