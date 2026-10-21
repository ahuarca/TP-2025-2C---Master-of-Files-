#ifndef QUERY_CONTROL_H_
#define QUERY_CONTROL_H_

#include "./gestor_queryControl.h"

t_log* queryControl_logger;
t_config* queryControl_config;

void iniciar_QueryControl();
void cargar_config();
void establecer_conexion();
void cargar_argumentos(char* argv[]);
void enviar_handshake();
void* gestionar_master();

char* IP_MASTER;
char* PUERTO_MASTER;
char* LOG_LEVEL;

char* archivo_config;
char* archivo_query;
int prioridad;

int fd_conexion;

#endif