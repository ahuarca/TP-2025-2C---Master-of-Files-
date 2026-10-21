#ifndef CONEXIONES_CON_MODULOS_H_
#define CONEXIONES_CON_MODULOS_H_


#include "utils_de_master.h"


void* conexiones_con_clientes();
void* espera_de_rta_de_worker(void* workerr);
void* espera_de_rta_de_queryControl(void* queryy);

#endif