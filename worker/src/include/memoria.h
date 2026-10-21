#ifndef MEMORIA_H_
#define MEMORIA_H_

#include "gestor_worker.h"
#include <commons/collections/list.h>
#include <math.h>
#include <commons/temporal.h>

typedef struct {
    char* file;
    char* tag;
    t_list* tabla_de_paginas;
} t_file_tag;

typedef struct {
    int pid;
    t_list* instrucciones;
    t_list* files_tags;
} t_proceso;


typedef struct{
    int nro_pagina;
    int nro_marco;
    int bit_modificado;
    int bit_uso;
    char* file;
    char* tag;
} t_pagina;


void iniciar_memoria();
void buscar_archivo(int pid, char* path);
t_list* generar_lista_de_instrucciones(char* path);
bool proceso_en_memoria(int pid);
t_proceso* buscar_proceso(t_list* lista, int pid);
char* obtener_instruccion(int pid, int pc);
void avanzar_clock();
bool asignar_marco(t_pagina* pagina, int pid, char* file, char*tag);
bool atender_storage_desde_memoria(int pid);
t_pagina* tiene_pagina_nro(t_file_tag* proceso_en_memoria,int pagina_nro);
char* escribir(int pid,int direccion_fisica, char* contenido, char* file, char* tag);
char* leer(int pid,int direccion_fisica, int tamanio, char* file, char* tag);
void flush(int pid, char* file, char* tag);
void liberar_proceso();
int algoritmo_de_reemplazo();
int algoritmo_lru();
int algoritmo_clock_m();
void commit(int pid);
t_file_tag* obtener_o_crear_file_tag(t_proceso* proceso, char* file, char* tag);
bool pagina_en_marco(void* ptr);
void memoria_truncate(int pid, char* file, char*tag, int tamanio);
void registrar_file_tag(int pid, char* file, char* tag);
char* solicitar_pagina_storage(int pid, char* file, char* tag, int nro_pagina);
bool atender_respuesta_storage();
t_file_tag* buscar_file_tag(t_proceso* proceso, char* file, char* tag);
void liberar_marco(int nro_marco,char* file, char* tag, int pid);
void flush_pagina(int pid, char* file, char* tag, t_pagina* pagina);

#endif
