#ifndef UTILS_DE_MASTER_H_
#define UTILS_DE_MASTER_H_


#include "gestor_master.h"

void agregar_a_ready(query_t* query);
int calcular_nivel_multiprogramacion();
query_t* buscar_query_con_untimoWorker(int id_worker);


worker_t* buscar_worker_libre();
query_t* buscar_proximo_query();
void pasar_query_de_estado_a_otro(query_t* query_proximo,estados inicio,estados fin);
void sacar_query_de_estado(query_t* query, estados estado);
void poner_query_en_estado(query_t* query, estados fin);
void mandar_mensaje_a_worker(worker_t* worker,query_t* query);
void mensaje_al_query_control(op_code operacion, query_t* query);
query_t* query_con_menos_prioridad_en_worker(int prioridad);
worker_t* buscar_worker_con_id(int id_worker);
void mensaje_de_desalojo(query_t* query_a_cambiar);
bool chequeo_de_desalojo(query_t* query);
void* contador_de_cambio_de_prioridad(void* arg);
void liberar_query(query_t* query);
void liberar_worker(worker_t* worker);
void agregar_acorde_a_la_prioridad(query_t* query);
query_t* buscar_query_con_id_en(int pid, estados estado);

void sacar_query_de_algun_estado(query_t* query);
bool esta_query_en_READY(int id);
bool esta_query_en_EXIT(int id);
bool esta_query_en_EXEC(int id);
void mensaje_de_desalojo_por_desconexion(query_t* query_cambiar);
void destruir_worker_conectada(void* workerV);
void destruir_queries(void* queryV);
void manejar_ctrl_c(int sig);
bool sigue_en_ready(query_t* query);


#endif