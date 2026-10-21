#ifndef INICIALIZADOR_H_
#define INICIALIZADOR_H_

#include "./gestor_worker.h"

void iniciar_worker();
void cargar_config();
void crear_logger();
void cargar_argumentos(char* argv[]);
void conectar_con_storage();
void conectar_con_master();

#endif