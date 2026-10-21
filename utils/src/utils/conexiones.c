#include "conexiones.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <commons/string.h>


//FUNCIONES INICIALIZADORAS DE LOGGER Y CONFIGS 

t_log* iniciar_logger(char* direccion, char* infoContenida, char* log_lvl){
	t_log* nuevo_logger = log_create(direccion,infoContenida,1,log_level_from_string(log_lvl));

	if (nuevo_logger==NULL){
		perror("Error creando logger");
		exit(EXIT_FAILURE);
	}
	return nuevo_logger;
}

t_config* iniciar_config(char* direccion){
	t_config* nuevo_config = config_create(direccion);
	if (nuevo_config==NULL){
		perror("Error cargando config");
		exit(EXIT_FAILURE);
	}
	return nuevo_config;
}

//FUNCIONALIDADES DEL CLIENTE 
int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family,
                         		server_info->ai_socktype,
                         		server_info->ai_protocol);;

	// Ahora que tenemos el socket, vamos a conectarlo
	int err = connect(socket_cliente, 
					  server_info->ai_addr,
					  server_info->ai_addrlen);
	if (err == -1) {
		perror("Error al conectar el socket");
		close(socket_cliente);
		freeaddrinfo(server_info);
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(server_info);

	return socket_cliente;
}





//FUNCIONALIDADES DEL SERVER 
int iniciar_servidor(int puerto, t_log* logger, char* mensaje)
{

	int socket_servidor;
	int err;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, string_itoa(puerto), &hints, &servinfo);

	if(err!= 0){
		printf("Error en la getaddrinfo del server : %s", gai_strerror(err));
		exit(-1);
	}

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family,
							servinfo->ai_socktype,
							servinfo->ai_protocol);

	//esta opcion permite que varios sockets se puedan bindear a un puerto 
	err = setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	if(err == -1){
		printf("Error en la setsockopt del server : %s ", strerror(errno));
		exit(-1);
	}

	err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	if (err == -1) {
        printf("Error en la bind del server: %s\n", strerror(errno));
        exit(-1);
    }

	err = listen(socket_servidor, SOMAXCONN);

	if (err == -1) {
        printf("Error en el listen del server: %s\n", strerror(errno));
        exit(-1);
    }


	freeaddrinfo(servinfo);
	log_info(logger, "## SERVER: %s",mensaje);

	return socket_servidor;
}



int esperar_cliente(int socket_servidor)
{

	// Aceptamos un nuevo cliente
	int socket_cliente;
	socket_cliente = accept(socket_servidor,NULL,NULL);

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
	{
		return cod_op;
	}
	close(socket_cliente);
	printf("Se cerro la conexión con el cliente\n");
	return -1;
}

t_buffer* crear_buffer(){
	t_buffer* un_buffer = malloc(sizeof(t_buffer));
	un_buffer->size =0;
	un_buffer->stream =NULL;

	return un_buffer;
}
void destruir_buffer(t_buffer* un_buffer){
	if(un_buffer->stream != NULL){
		free(un_buffer->stream);
	}
	free(un_buffer);
}

t_paquete* crear_paquete(op_code codigo, t_buffer* un_buffer){
	t_paquete* un_paquete = malloc(sizeof(t_paquete));
	un_paquete->codigo_operacion = codigo;
	un_paquete->buffer = un_buffer;

	return un_paquete;
}
void eliminar_paquete(t_paquete* paquete) {
    if (paquete != NULL) {
        destruir_buffer(paquete->buffer);
        free(paquete);
    }
}
/*
void* serializar_paquete(t_paquete* un_paquete){
	int size_coso = un_paquete->buffer != NULL ? un_paquete->buffer->size : 0 + 2*sizeof(int);
	void* coso = malloc(size_coso); 
	int desplazamiento=0;

	memcpy(coso + desplazamiento,&(un_paquete->codigo_operacion),sizeof(int)); 
	desplazamiento+=sizeof(int);

	if (un_paquete->buffer != NULL)
	{
		memcpy(coso + desplazamiento,&(un_paquete->buffer->size),sizeof(int));
		desplazamiento+=sizeof(int);
		memcpy(coso + desplazamiento,un_paquete->buffer->stream,un_paquete->buffer->size);
	} else
	{
		const int zero = 0;
		memcpy(coso + desplazamiento,&zero,sizeof(int));
	}

	return coso;
}
*/
void* serializar_paquete(t_paquete* un_paquete){
	int size_coso = un_paquete->buffer->size + 2*sizeof(int); 
	void* coso = malloc(size_coso); 
	int desplazamiento=0;

	memcpy(coso + desplazamiento,&(un_paquete->codigo_operacion),sizeof(int)); 
	desplazamiento+=sizeof(int); 
	memcpy(coso + desplazamiento,&(un_paquete->buffer->size),sizeof(int)); 
	desplazamiento+=sizeof(int); 
	memcpy(coso + desplazamiento,un_paquete->buffer->stream,un_paquete->buffer->size);
	desplazamiento+=un_paquete->buffer->size; 

	return coso;
}

void enviar_paquete(t_paquete* un_paquete, int conexion){
	void* a_enviar = serializar_paquete(un_paquete);
	int bytes = un_paquete->buffer->size + 2*sizeof(int);
	send(conexion,a_enviar,bytes,0);
	free(a_enviar);
}

void enviar_handshake_especial(op_code codigo, int conexion){
	send(conexion,&codigo,sizeof(int),0);
}

//funciones cargar a un buffer 
void cargar_choclo_al_buffer(t_buffer* un_buffer,void* choclo, int size_choclo){
	if(un_buffer->size==0){
		un_buffer->stream =malloc(sizeof(int) + size_choclo);
		memcpy(un_buffer->stream, &size_choclo, sizeof(int));
		memcpy(un_buffer->stream  +sizeof(int), choclo,size_choclo);
	}
	else{
		un_buffer->stream =realloc(un_buffer->stream, un_buffer->size +sizeof(int) + size_choclo);
		memcpy(un_buffer->stream + un_buffer->size ,&size_choclo,sizeof(int));
		memcpy(un_buffer->stream + un_buffer->size + sizeof(int), choclo,size_choclo);
	}
	un_buffer->size += size_choclo + sizeof(int);  
}

void cargar_entero_al_buffer(t_buffer* un_buffer, int value){
	cargar_choclo_al_buffer(un_buffer,&value,sizeof(int));
}
void cargar_string_al_buffer(t_buffer* un_buffer, char* un_string){
	cargar_choclo_al_buffer(un_buffer, un_string, strlen(un_string)+1);
}
void cargar_uint32_al_buffer(t_buffer* un_buffer,uint32_t value){
	cargar_choclo_al_buffer(un_buffer,&value,sizeof(uint32_t));
}
void cargar_double_al_buffer(t_buffer* un_buffer, double value){
	cargar_choclo_al_buffer(un_buffer,&value,sizeof(double));
}

// Funciones para recibir datos 

t_buffer* recibir_todo_un_buffer(int fd) {
    int size_choclo;

    // Recibir el tamaño del buffer (primer entero)
    if (recv(fd, &size_choclo, sizeof(int), MSG_WAITALL) <= 0) {
        perror("Error al recibir el tamaño del buffer");
        close(fd);
        exit(EXIT_FAILURE);
    }
	if (size_choclo == 0)
	{
		printf("[WARN] Se recivio un paquete con tamaño = 0\n");
		t_buffer* un_buffer = crear_buffer();
		un_buffer->size = 0;
		un_buffer->stream = NULL;

		return un_buffer;
	}
    // Asignar memoria para recibir el stream de datos
    void* choclo = malloc(size_choclo);
    if (choclo == NULL) {
        perror("Error al asignar memoria para recibir datos");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Recibir los datos del buffer
    if (recv(fd, choclo, size_choclo, MSG_WAITALL) <= 0) {
        free(choclo); // Liberar la memoria si no se recibe correctamente
        perror("Error al recibir los datos del buffer");
        close(fd);
        exit(EXIT_FAILURE);
    }

    t_buffer* un_buffer = crear_buffer();
    un_buffer->size = size_choclo;
    un_buffer->stream = choclo;

    return un_buffer;
}

void* extraer_choclo_del_buffer(t_buffer* un_buffer){
	if(un_buffer->size==0){
		printf("[ERROR] Al intentar extraer un contenido de un t_buffer porque size_buffer = 0\n");
		exit(EXIT_FAILURE);
	}
	if(un_buffer->size <0){
		printf("[ERROR] El buffer tiene un size negativo\n");
		exit(EXIT_FAILURE);
	}
	int size_choclo;
	memcpy(&size_choclo,un_buffer->stream, sizeof(int));
	void *choclo =malloc(size_choclo);
	memcpy(choclo,un_buffer->stream + sizeof(int), size_choclo);

	int nuevo_size= un_buffer->size -sizeof(int) - size_choclo;
	if(nuevo_size ==0){
		un_buffer->size=0;
		free(un_buffer->stream);
		un_buffer->stream =NULL;
		return choclo;
	}
	if(nuevo_size<0){
		perror("[ERROR CHOCLO] BUFFER con tamanio negativo");
		exit(EXIT_FAILURE);
	}
	void *nuevo_stream = malloc(nuevo_size);
	memcpy(nuevo_stream, un_buffer->stream + sizeof(int) +size_choclo,nuevo_size);
	free(un_buffer->stream);
	un_buffer->size =nuevo_size;
	un_buffer->stream = nuevo_stream;

	return choclo;
}

int extraer_int_del_buffer(t_buffer* buffer){
	int* un_entero= extraer_choclo_del_buffer(buffer);
	int valor_retorno= *un_entero;
	free(un_entero);
	return valor_retorno;
}

char* extraer_string_del_buffer(t_buffer* buffer) {
    int size_choclo;
    memcpy(&size_choclo, buffer->stream, sizeof(int));

    char* string = malloc(size_choclo + 1);  // +1 para asegurarnos espacio extra
    memcpy(string, buffer->stream + sizeof(int), size_choclo);
    string[size_choclo] = '\0';

    // Actualizar buffer
    int nuevo_size = buffer->size - sizeof(int) - size_choclo;
    if (nuevo_size < 0) {
        perror("[ERROR STRING] Tamaño negativo al extraer");
        exit(EXIT_FAILURE);
    }

    void* nuevo_stream = (nuevo_size > 0) ? malloc(nuevo_size) : NULL;
    if (nuevo_size > 0)
        memcpy(nuevo_stream, buffer->stream + sizeof(int) + size_choclo, nuevo_size);

    free(buffer->stream);
    buffer->stream = nuevo_stream;
    buffer->size = nuevo_size;

    return string;
}


uint32_t extraer_uint32_del_buffer(t_buffer* buffer){
	uint32_t* un_valor= extraer_choclo_del_buffer(buffer);
	uint32_t valor_retorno= *un_valor;
	free(un_valor);
	return valor_retorno;
}
double extraer_double_del_buffer(t_buffer* buffer){
	int* un_double= extraer_choclo_del_buffer(buffer);
	double valor_retorno= *un_double;
	free(un_double);
	return valor_retorno;
}

void enviar_hand_shake_cpu(op_code operacion, int fd_server, int identificador){
	
    t_buffer* buffer= crear_buffer();
    cargar_entero_al_buffer(buffer,identificador);
    t_paquete* paquete = crear_paquete(operacion, buffer);
    enviar_paquete(paquete,fd_server);
    eliminar_paquete(paquete);
}


void enviar_hand_shake_kernel_mem(op_code operacion, int fd_server){
    t_buffer* buffer= crear_buffer();
	cargar_entero_al_buffer(buffer, 1);
    t_paquete* paquete = crear_paquete(operacion, buffer);
    enviar_paquete(paquete,fd_server);
    eliminar_paquete(paquete);
}

void eliminar_buffer(t_buffer *buffer)
{
	if (buffer != NULL)
		free(buffer->stream);

	free(buffer);
}

t_paquete *iniciar_paquete(int cod_op)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->codigo_operacion = cod_op;
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
	return paquete;
}

