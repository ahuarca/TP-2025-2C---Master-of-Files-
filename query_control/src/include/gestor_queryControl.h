#ifndef GESTOR_QUERY_CONTROL_H_
#define GESTOR_QUERY_CONTROL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include "../../utils/src/utils/conexiones.h"

extern t_log* queryControl_logger;
extern t_config* queryControl_config;

extern char* IP_MASTER;
extern char* PUERTO_MASTER;
extern char* LOG_LEVEL;

extern char* archivo_config;
extern char* archivo_query;
extern int prioridad;

extern int fd_conexion;

#endif