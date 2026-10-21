#ifndef QUERY_INTERPRETER_H_
#define QUERY_INTERPRETER_H_

#include "./gestor_worker.h"
#include "./memoria.h"

typedef struct{
    int pid;
    int pc;
    char* path_proceso;
}t_proceso_query;



typedef struct{
    char* operacion;
    char* nombre_file;
    char* tag;
    int primer_parametro_entero;
    int segundo_parametro_entero;
    char* primer_parametro_cadena;
    char* segundo_parametro_cadena;
}t_instruccion;

t_proceso_query* proceso = NULL;
t_instruccion* instruccion = NULL;
bool hay_interrupcion = false;
char* instruccion_completa;

pthread_mutex_t sem_hay_interrupcion;

void iniciar_query_interpreter();
void recibir_query();

void ciclo_de_instruccion();
void fetch();
void decode();
void execute();

void escuchar_interrupciones();
void* check_interrupt(void* arg);
bool hay_una_interrupcion();

void enviar_contexto();
void destruir_instruccion();
bool atender_respuesta_storage();
void liberar_proceso();
void enviar_datos_a_master(int pid,char* nombre_file,char* nombre_tag,char* lectura);
#endif