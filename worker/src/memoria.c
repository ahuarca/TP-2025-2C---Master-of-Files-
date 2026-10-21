#include "./include/memoria.h"
#include <unistd.h>

t_marco* marcos_de_memoria = NULL;
pthread_mutex_t procesos_mutex;
t_marco* puntero_clock = NULL;
t_marco* puntero_flush = NULL;
t_list* procesos=NULL;
int CANT_MARCOS = 0;
int puntero_index = 0;

void iniciar_memoria(){

    memoria_interna = malloc(TAM_MEMORIA);
    memset(memoria_interna, 0, TAM_MEMORIA);
    
    CANT_MARCOS = TAM_MEMORIA/TAM_PAGINA;

    marcos_de_memoria = malloc(CANT_MARCOS*sizeof(t_marco));

    for (int i = 0; i < CANT_MARCOS; i++)
    {
        marcos_de_memoria[i].ocupado = 0;
        marcos_de_memoria[i].nro_pagina = -1;
        marcos_de_memoria[i].proceso_id = -1;
        marcos_de_memoria[i].ultima_referencia = temporal_create();
    }

    puntero_clock = &marcos_de_memoria[0];
    puntero_flush = &marcos_de_memoria[puntero_index];

    // Lista de procesos con sus path

    pthread_mutex_init(&procesos_mutex,NULL);

    procesos = list_create();
}

void buscar_archivo(int pid, char* path) {

    t_proceso* proceso = buscar_proceso(procesos, pid);

    if (!proceso) {
        proceso = malloc(sizeof(t_proceso));
        proceso->pid = pid;
        proceso->instrucciones = generar_lista_de_instrucciones(path);
        proceso->files_tags = list_create();

        pthread_mutex_lock(&procesos_mutex);
        list_add(procesos, proceso);
        pthread_mutex_unlock(&procesos_mutex);
    }

}


t_list* generar_lista_de_instrucciones(char* path){

    char archivo_de_instrucciones[100];

    strcpy(archivo_de_instrucciones,PATH_SCRIPTS);
    strcat(archivo_de_instrucciones,path);

    FILE* archivo = fopen(archivo_de_instrucciones,"r");

    if (archivo == NULL) {
        log_error(worker_logger, "Error al abrir el archivo: %s. Finalizando ejecucion...", archivo_de_instrucciones);
        exit(EXIT_FAILURE);
        return NULL;
    }

    t_list* lista_de_instrucciones = list_create();
   
    char linea[100];

    while(fgets(linea, sizeof(linea), archivo) != NULL){

        linea[strcspn(linea, "\n")] = '\0';
        
        list_add(lista_de_instrucciones,strdup(linea));

    }

    fclose(archivo);

    return lista_de_instrucciones;
    
}


bool proceso_en_memoria(int pid) {

    pthread_mutex_lock(&procesos_mutex);
    bool existe = buscar_proceso(procesos, pid) != NULL;
    pthread_mutex_unlock(&procesos_mutex);

    return existe;
}


t_proceso* buscar_proceso(t_list* lista, int pid){
    
    bool mismo_pid(void* ptr) {
        t_proceso* proceso_buscado = (t_proceso*) ptr;
        return proceso_buscado->pid == pid;
    };

    pthread_mutex_lock(&procesos_mutex);
    t_proceso* proceso_guardado = list_find(lista,mismo_pid);
    pthread_mutex_unlock(&procesos_mutex);

    return proceso_guardado;
}

char* obtener_instruccion(int pid, int pc) {

    t_proceso* proceso_guardado = buscar_proceso(procesos, pid);

    if (!proceso_guardado) return NULL;

    if (pc < 0 || pc >= list_size(proceso_guardado->instrucciones)) return NULL;

    return list_get(proceso_guardado->instrucciones, pc);
}

char* escribir(int pid, int direccion_fisica, char* contenido, char* file, char* tag) {


    t_proceso* proceso = buscar_proceso(procesos, pid);
    if (!proceso) return NULL;

    t_file_tag* ft = obtener_o_crear_file_tag(proceso, file, tag);
    if (!ft) return NULL;

    int pagina_nro = direccion_fisica / TAM_PAGINA;
    int desplazamiento = direccion_fisica % TAM_PAGINA;

    int tamanio_texto = strlen(contenido);
    int escrito = 0;

    while (escrito < tamanio_texto) {

        t_pagina* pagina = tiene_pagina_nro(ft, pagina_nro);

        if (!pagina) {

            log_info(worker_logger, "## Query <%d>: Memoria Miss - File <%s> - Tag <%s> - Página <%d>", pid, file, tag, pagina_nro);

            pagina = malloc(sizeof(t_pagina));
            pagina->nro_pagina = pagina_nro;
            pagina->nro_marco  = -1;
            pagina->bit_modificado = 0;
            pagina->bit_uso = 1;
            pagina->file = strdup(file);
            pagina->tag  = strdup(tag);
            list_add(ft->tabla_de_paginas, pagina);
        }

        if (pagina->nro_marco == -1) {

            log_info(worker_logger, "## Query <%d>: Memoria Miss - File <%s> - Tag <%s> - Página <%d>", pid, file, tag, pagina_nro);

            char* contenido_storage = solicitar_pagina_storage(pid, file, tag, pagina_nro);

            if (!contenido_storage) {
                log_error(worker_logger, "Error al obtener página <%d> desde storage", pagina_nro);
                return NULL;
            }

            if (!asignar_marco(pagina, pid, file, tag)) {
                log_error(worker_logger, "No se pudo asignar marco a la página <%d>", pagina_nro);
                free(contenido_storage);
                return NULL;
            }

            memcpy(memoria_interna + pagina->nro_marco * TAM_PAGINA, contenido_storage, TAM_PAGINA);

            free(contenido_storage);

            log_info(worker_logger, "## Query <%d>: Memoria Add - Página <%d> - Marco <%d>", pid, pagina_nro, pagina->nro_marco);
        }

        pagina->bit_modificado = 1;
        pagina->bit_uso = 1;

        if (marcos_de_memoria[pagina->nro_marco].ultima_referencia != NULL)
            temporal_destroy(marcos_de_memoria[pagina->nro_marco].ultima_referencia);

        marcos_de_memoria[pagina->nro_marco].ultima_referencia = temporal_create();

        int direccion_mem = (pagina->nro_marco * TAM_PAGINA) + desplazamiento;
        int espacio_disponible = TAM_PAGINA - desplazamiento;
        int a_escribir;

        if ((tamanio_texto - escrito) < espacio_disponible) {
            a_escribir = tamanio_texto - escrito;
        } else {
            a_escribir = espacio_disponible;
        }

        usleep(RETARDO_MEMORIA * 1000);

        memcpy(memoria_interna + direccion_mem, contenido + escrito, a_escribir);

        escrito += a_escribir;
        pagina_nro++;
        desplazamiento = 0;
    }

    return strdup(contenido);
}




t_pagina* tiene_pagina_nro(t_file_tag* ft, int nro_pag) {
    bool mismo_num(void* ptr) {
        t_pagina* pagina = (t_pagina*) ptr;
        return pagina->nro_pagina == nro_pag;
    }
    return list_find(ft->tabla_de_paginas, mismo_num);
}


char* leer(int pid, int direccion_fisica, int tamanio, char* file, char* tag) {

    t_proceso* proceso_en_memoria = buscar_proceso(procesos, pid);
    if (!proceso_en_memoria) {
        log_error(worker_logger, "Proceso <%d> no encontrado en memoria", pid);
        return NULL;
    }

    t_file_tag* ft = obtener_o_crear_file_tag(proceso_en_memoria, file, tag);
    if (!ft) return NULL;

    int pagina_nro = direccion_fisica / TAM_PAGINA;
    int desplazamiento = direccion_fisica % TAM_PAGINA;
    int leido = 0;

    char* resultado = malloc(tamanio + 1);

    while (leido < tamanio) {


        t_pagina* pagina = tiene_pagina_nro(ft, pagina_nro);

        // Página no está en memoria: traer desde Storage
        if (!pagina) {

            log_info(worker_logger, "## Query <%d>: Memoria Miss - File <%s> - Tag <%s> - Página <%d>", pid, file, tag, pagina_nro);

            char* contenido_storage = solicitar_pagina_storage(pid, file, tag, pagina_nro);

            if (!contenido_storage) {
                log_error(worker_logger, "No se pudo obtener página <%d> desde storage", pagina_nro);
                free(resultado);
                return NULL;
            }

            pagina = malloc(sizeof(t_pagina));
            pagina->nro_pagina = pagina_nro;
            pagina->bit_modificado = 0;
            pagina->bit_uso = 1;
            pagina->file = strdup(file);
            pagina->tag = strdup(tag);
            pagina->nro_marco = -1;

            if (!asignar_marco(pagina, pid, file, tag)) {
                log_error(worker_logger, "No se pudo asignar marco a la página <%d>", pagina_nro);
                free(contenido_storage);
                free(pagina->file);
                free(pagina->tag);
                free(pagina);
                free(resultado);
                return NULL;
            }

            int direccion_en_memoria = pagina->nro_marco * TAM_PAGINA;
            memcpy(memoria_interna + direccion_en_memoria, contenido_storage, TAM_PAGINA);

            list_add(ft->tabla_de_paginas, pagina);
            free(contenido_storage);

            log_info(worker_logger, "## Query <%d>: Memoria Add - Página <%d> - Marco <%d>", pid, pagina_nro, pagina->nro_marco);
        }


        else if (pagina->nro_marco == -1) {

            log_info(worker_logger, "## Query <%d>: Memoria Miss - File <%s> - Tag <%s> - Página <%d>", pid, file, tag, pagina_nro);

            char* contenido = solicitar_pagina_storage(pid, file, tag, pagina_nro);
            if (!contenido) {
                log_error(worker_logger, "No se pudo obtener página <%d> desde storage", pagina_nro);
                free(resultado);
                return NULL;
            }

            if (!asignar_marco(pagina, pid, file, tag)) {
                log_error(worker_logger, "No se pudo asignar marco a la página <%d>", pagina_nro);
                free(contenido);
                free(resultado);
                return NULL;
            }

            memcpy(memoria_interna + pagina->nro_marco * TAM_PAGINA, contenido, TAM_PAGINA);

            free(contenido);

            pagina->bit_uso = 1;
            pagina->bit_modificado = 0;

            log_info(worker_logger, "## Query <%d>: Memoria Add - Página <%d> - Marco <%d>", pid, pagina_nro, pagina->nro_marco);

        } else {
            pagina->bit_uso = 1;
        }

        usleep(RETARDO_MEMORIA * 1000);

        if (marcos_de_memoria[pagina->nro_marco].ultima_referencia != NULL) {
            temporal_destroy(marcos_de_memoria[pagina->nro_marco].ultima_referencia);
        }

        marcos_de_memoria[pagina->nro_marco].ultima_referencia = temporal_create();

        int direccion_en_memoria = (pagina->nro_marco * TAM_PAGINA) + desplazamiento;
        int espacio_disponible = TAM_PAGINA - desplazamiento;
        int a_leer;

        if (tamanio - leido < espacio_disponible) {
            a_leer = tamanio - leido;
        } else {
            a_leer = espacio_disponible;
        }

        memcpy(resultado + leido, memoria_interna + direccion_en_memoria, a_leer);

        leido += a_leer;
        pagina_nro++;
        desplazamiento = 0;
    }

    resultado[tamanio] = '\0';
    return resultado;
}




void flush(int pid, char* file, char* tag) {
    t_proceso* proceso = buscar_proceso(procesos, pid);
    if (!proceso) return;

    for (int i = 0; i < list_size(proceso->files_tags); i++) {
        t_file_tag* ft = list_get(proceso->files_tags, i);

        if (strcmp(ft->file, file) != 0 || strcmp(ft->tag, tag) != 0) continue;

        for (int j = 0; j < list_size(ft->tabla_de_paginas); j++) {
            t_pagina* pagina = list_get(ft->tabla_de_paginas, j);

            if (!pagina->bit_modificado) continue;

            char* contenido = malloc(TAM_PAGINA);
            memcpy(contenido, memoria_interna + pagina->nro_marco * TAM_PAGINA, TAM_PAGINA);

            t_buffer* buffer_flush = crear_buffer();
            cargar_string_al_buffer(buffer_flush, ft->file);
            cargar_string_al_buffer(buffer_flush, ft->tag);
            cargar_entero_al_buffer(buffer_flush, pagina->nro_pagina);
            cargar_string_al_buffer(buffer_flush, contenido);
            cargar_entero_al_buffer(buffer_flush, pid);

            t_paquete* paquete_flush = crear_paquete(SOL_WRITE, buffer_flush);
            enviar_paquete(paquete_flush, fd_storage);
            eliminar_paquete(paquete_flush);
            free(contenido);

            bool exito = atender_storage_desde_memoria(pid);
            if (!exito) return;

            pagina->bit_modificado = 0;
        }
    }
}

bool asignar_marco(t_pagina* pagina, int pid, char* file, char* tag) {

    for (int i = 0; i < CANT_MARCOS; i++) {
        if (marcos_de_memoria[i].ocupado == 0) {

            pagina->nro_marco = i;
            marcos_de_memoria[i].ocupado = 1;
            marcos_de_memoria[i].proceso_id = pid;
            marcos_de_memoria[i].nro_pagina = pagina->nro_pagina;

            if (marcos_de_memoria[i].ultima_referencia != NULL)
                temporal_destroy(marcos_de_memoria[i].ultima_referencia);
            marcos_de_memoria[i].ultima_referencia = temporal_create();

            log_info(worker_logger,
                "## Query <%d>: Se asigna el Marco: <%d> a la Página: <%d> - File <%s> - Tag <%s>",
                pid, pagina->nro_marco, pagina->nro_pagina, file, tag);

            return true;
        }
    }

    int marco_elegido = algoritmo_de_reemplazo();
    t_marco* marco_a_liberar = &marcos_de_memoria[marco_elegido];

    t_proceso* proceso_en_marco = buscar_proceso(procesos, marco_a_liberar->proceso_id);
    if (!proceso_en_marco) {
        log_error(worker_logger, "No se encontró proceso dueño del marco <%d>", marco_elegido);
        return false;
    }

    t_file_tag* file_tag_en_marco = NULL;
    t_pagina* pagina_en_marco = NULL;

    for (int i = 0; i < list_size(proceso_en_marco->files_tags); i++) {
        t_file_tag* ft = list_get(proceso_en_marco->files_tags, i);
        pagina_en_marco = tiene_pagina_nro(ft, marco_a_liberar->nro_pagina);
        if (pagina_en_marco != NULL) {
            file_tag_en_marco = ft;
            break;
        }
    }

    if (!file_tag_en_marco || !pagina_en_marco) {
        log_error(worker_logger,
            "No se encontró página a liberar en el proceso <%d>, marco <%d>", pid, marco_elegido);
        return false;
    }

    if (pagina_en_marco->bit_modificado == 1) {
        char* contenido_en_memoria = malloc(TAM_PAGINA);
        memcpy(contenido_en_memoria, memoria_interna + TAM_PAGINA * marco_elegido, TAM_PAGINA);

        t_buffer* buffer_nuevo = crear_buffer();
        cargar_string_al_buffer(buffer_nuevo, file_tag_en_marco->file);
        cargar_string_al_buffer(buffer_nuevo, file_tag_en_marco->tag);
        cargar_entero_al_buffer(buffer_nuevo, pagina_en_marco->nro_pagina);
        cargar_string_al_buffer(buffer_nuevo, contenido_en_memoria);
        cargar_entero_al_buffer(buffer_nuevo, proceso_en_marco->pid);

        t_paquete* paquete_nuevo = crear_paquete(SOL_WRITE, buffer_nuevo);
        enviar_paquete(paquete_nuevo, fd_storage);
        eliminar_paquete(paquete_nuevo);

        bool exito = atender_storage_desde_memoria(pid);
        free(contenido_en_memoria);

        if(!exito) 
            return false;

        pagina_en_marco->bit_modificado = 0;
    }

    pagina_en_marco->nro_marco = -1;
    pagina_en_marco->bit_uso = 0;
    pagina_en_marco->bit_modificado = 0;

    log_info(worker_logger,
        "## Query <%d>: Se reemplaza la página <%s:%s>/<%d> por la <%s:%s>/<%d>",
        pid,
        file_tag_en_marco->file,
        file_tag_en_marco->tag,
        marco_a_liberar->nro_pagina,
        file,
        tag,
        pagina->nro_pagina);

    if (marco_a_liberar->ultima_referencia != NULL)
        temporal_destroy(marco_a_liberar->ultima_referencia);

    marco_a_liberar->ultima_referencia = temporal_create();
    marco_a_liberar->proceso_id = pid;
    marco_a_liberar->nro_pagina = pagina->nro_pagina;
    pagina->nro_marco = marco_elegido;

    return true;

}

int algoritmo_de_reemplazo(){
    if(strcmp(ALGORITMO_REEMPLAZO,"LRU")==0){
        return algoritmo_lru();
    }else if(strcmp(ALGORITMO_REEMPLAZO,"CLOCK-M")==0){
        return algoritmo_clock_m();
    }else{
        log_error(worker_logger,"Algoritmo de reemplazo no reconocido, finalizando ejecucion...");
        exit(EXIT_FAILURE);
    }
}

int algoritmo_lru(){

    for(int i = 0; i < CANT_MARCOS; i++){
        temporal_stop(marcos_de_memoria[i].ultima_referencia);
    }

    t_marco* marco_victima = &marcos_de_memoria[0];
    int marco_a_liberar = 0;

    for (int j = 1; j < CANT_MARCOS; j++)
    {
        t_marco* marco_actual = &marcos_de_memoria[j];

        if(temporal_gettime(marco_actual->ultima_referencia) > temporal_gettime(marco_victima->ultima_referencia)){
            marco_victima = marco_actual;
            marco_a_liberar = j;
        }

    }
    
    for(int k = 0; k < CANT_MARCOS; k++){
        temporal_resume(marcos_de_memoria[k].ultima_referencia);
    }

    return marco_a_liberar;

}

int algoritmo_clock_m() {

    for (int vuelta = 1; vuelta <= 4; vuelta++) {

        for (int k = 0; k < CANT_MARCOS; k++) {

            int i = puntero_index;
            t_marco* marco = &marcos_de_memoria[i];

            if (marco->ocupado == 0) {
                avanzar_clock();
                continue;
            }

            t_proceso* proceso = buscar_proceso(procesos, marco->proceso_id);
            if (!proceso) {
                avanzar_clock();
                continue;
            }

            t_pagina* pagina = NULL;
            for (int ft_i = 0; ft_i < list_size(proceso->files_tags); ft_i++) {
                t_file_tag* ft = list_get(proceso->files_tags, ft_i);
                pagina = list_find(ft->tabla_de_paginas, pagina_en_marco);
                if (pagina) break;
            }

            if (!pagina) {
                avanzar_clock();
                continue;
            }

            int uso = pagina->bit_uso;
            int mod = pagina->bit_modificado;

            // --- PASADA 1 y 3: buscar (0,0) ---
            if ((vuelta == 1 || vuelta == 3) && uso == 0 && mod == 0) {
                int victima = i;
                avanzar_clock();
                return victima;
            }

            // --- PASADA 2 y 4 ---
            if (vuelta == 2 || vuelta == 4) {

                // Buscar (0,1)
                if (uso == 0 && mod == 1) {
                    int victima = i;
                    avanzar_clock();
                    return victima;
                }

                if (uso == 1) {
                    pagina->bit_uso = 0;
                }
            }

            avanzar_clock();
        }
    }

    int victima = puntero_index;
    avanzar_clock();
    return victima;
}

bool pagina_en_marco(void* ptr) {
    t_pagina* pagina = (t_pagina*) ptr;
    return pagina->nro_marco == puntero_index;  // Compara el nro_marco de la página con el índice del puntero
}

void avanzar_clock() {
    puntero_index = (puntero_index + 1) % CANT_MARCOS;
    puntero_clock = &marcos_de_memoria[puntero_index];
}

bool atender_storage_desde_memoria(int pid){

    int codigo = recibir_operacion(fd_storage);

    switch (codigo){
    case RTA_CREATE:
	case RTA_TRUNCATE:
	case RTA_WRITE:
	case RTA_READ:
	case RTA_TAG:
	case RTA_COMMIT:
	case RTA_FLUSH:
	case RTA_DELETE:
        t_buffer * respuesta = recibir_todo_un_buffer(fd_storage);
        char* relleno = strdup(extraer_string_del_buffer(respuesta));
        eliminar_buffer(respuesta);
        return true;
    case FILE_INEXISTENTE:
    case FILE_PREEXISTENTE:
    case ESPACIO_INSUFICIENTE:
    case ESCRITURA_NO_PERMITIDA:
    case LECTURA_FUERA_DE_LIMITE:
    case ESCRITURA_FUERA_DE_LIMITE:

        t_buffer* buffer_rta = recibir_todo_un_buffer(fd_storage);
        char* motivo = strdup(extraer_string_del_buffer(buffer_rta));
        eliminar_buffer(buffer_rta);

        t_buffer* buffer_fin = crear_buffer();
        cargar_entero_al_buffer(buffer_fin,pid);
        cargar_string_al_buffer(buffer_fin,motivo);

        liberar_proceso();

        t_paquete* paquete = crear_paquete(FIN_POR_ERROR,buffer_fin);
        enviar_paquete(paquete,fd_master);
        eliminar_paquete(paquete);
        free(motivo);
        return false;

    default:
        exit(EXIT_FAILURE);
        break;
    }

}

void commit(int pid) {
    t_proceso* proceso = buscar_proceso(procesos, pid);
    if (!proceso) return;

    // Flush por cada file_tag modificado
    for (int i = 0; i < list_size(proceso->files_tags); i++) {
        t_file_tag* ft = list_get(proceso->files_tags, i);
        flush(pid, ft->file, ft->tag);
        log_info(worker_logger, "## Query <%d>: EXECUTE - FLUSH File: <%s> Tag: <%s>",
                 pid, ft->file, ft->tag);
    }

    // Commit de cada página en cada file_tag
    for (int i = 0; i < list_size(proceso->files_tags); i++) {
        t_file_tag* ft = list_get(proceso->files_tags, i);

        for (int j = 0; j < list_size(ft->tabla_de_paginas); j++) {
            t_pagina* pagina = list_get(ft->tabla_de_paginas, j);

            t_buffer* buffer = crear_buffer();
            cargar_string_al_buffer(buffer, ft->file);
            cargar_string_al_buffer(buffer, ft->tag);
            cargar_entero_al_buffer(buffer, pid);

            t_paquete* paquete = crear_paquete(SOL_TAG_COMMIT, buffer);
            enviar_paquete(paquete, fd_storage);
            eliminar_paquete(paquete);
            atender_respuesta_storage();
        }
    }
}



t_file_tag* buscar_file_tag(t_proceso* proceso, char* file, char* tag) {
    bool coincide(void* ptr) {
        t_file_tag* ft = (t_file_tag*)ptr;
        return strcmp(ft->file, file) == 0 && strcmp(ft->tag, tag) == 0;
    }
    return list_find(proceso->files_tags, coincide);
}

t_file_tag* obtener_o_crear_file_tag(t_proceso* proceso, char* file, char* tag) {
    t_file_tag* ft = buscar_file_tag(proceso, file, tag);
    if (!ft) {
        ft = malloc(sizeof(t_file_tag));
        ft->file = strdup(file);
        ft->tag = strdup(tag);
        ft->tabla_de_paginas = list_create();
        list_add(proceso->files_tags, ft);
    }
    return ft;
}

void memoria_truncate(int pid, char* file, char* tag, int nuevo_tamanio) {

    t_proceso* proceso_mem = buscar_proceso(procesos, pid);
    if (!proceso_mem) return;

    t_file_tag* ft = buscar_file_tag(proceso_mem, file, tag);
    if (!ft) return;

    int tam_pagina = TAM_PAGINA;

    int nuevas_paginas = (nuevo_tamanio + tam_pagina - 1) / tam_pagina;
    int paginas_actuales = list_size(ft->tabla_de_paginas);

    if (nuevas_paginas < paginas_actuales) {

        for (int i = paginas_actuales - 1; i >= nuevas_paginas; i--) {

            t_pagina* pag = list_get(ft->tabla_de_paginas, i);

            if (pag->nro_marco != -1) {

                if (pag->bit_modificado == 1) {
                    flush_pagina(pid, file, tag, pag);
                }

                liberar_marco(pag->nro_marco,file,tag,pid);
            }

            list_remove_and_destroy_element(ft->tabla_de_paginas,i,free);
        }
    }

    else if (nuevas_paginas > paginas_actuales) {

        for (int i = paginas_actuales; i < nuevas_paginas; i++) {

            t_pagina* nueva = malloc(sizeof(t_pagina));
            nueva->nro_pagina = i;
            nueva->nro_marco = -1;
            nueva->bit_modificado = 0;
            nueva->bit_uso = 0;
            nueva->file = strdup(file);
            nueva->tag  = strdup(tag);

            list_add(ft->tabla_de_paginas, nueva);
        }
    }
}

void flush_pagina(int pid, char* file, char* tag, t_pagina* pagina) {

    if (pagina->nro_marco == -1 || pagina->bit_modificado == 0)
        return;

    char* contenido = malloc(TAM_PAGINA);
    memcpy(contenido, memoria_interna + pagina->nro_marco * TAM_PAGINA, TAM_PAGINA);

    t_buffer* buffer_flush = crear_buffer();
    cargar_string_al_buffer(buffer_flush, file);
    cargar_string_al_buffer(buffer_flush, tag);
    cargar_entero_al_buffer(buffer_flush, pagina->nro_pagina);
    cargar_string_al_buffer(buffer_flush, contenido);
    cargar_entero_al_buffer(buffer_flush, pid);

    t_paquete* paquete_flush = crear_paquete(SOL_WRITE, buffer_flush);
    enviar_paquete(paquete_flush, fd_storage);
    eliminar_paquete(paquete_flush);
    free(contenido);

    bool exito = atender_storage_desde_memoria(pid);
    if (!exito) {
        return;
    }

    pagina->bit_modificado = 0;
}


void liberar_marco(int nro_marco,char* file, char* tag, int pid) {
    if (nro_marco < 0 || nro_marco >= CANT_MARCOS) {
        log_debug(worker_logger, "Intento de liberar un marco inválido: %d", nro_marco);
        return;
    }

    t_marco* marco = &marcos_de_memoria[nro_marco];

    marco->ocupado = 0;
    marco->nro_pagina = -1;
    marco->proceso_id = -1;

    if (marco->ultima_referencia != NULL) {
        temporal_destroy(marco->ultima_referencia);
    }

    memset(memoria_interna + nro_marco * TAM_PAGINA, 0, TAM_PAGINA);

    log_info(worker_logger, "## Query <%d>: Se libera el Marco <%d> perteneciente al - FIle: <%s> - Tag: <%s>", pid, nro_marco,file,tag);
    log_debug(worker_logger, "Marco %d liberado", nro_marco);
}

void registrar_file_tag(int pid, char* file, char* tag) {

    t_proceso* proceso_mem = buscar_proceso(procesos,pid);

    t_file_tag* nuevo = malloc(sizeof(t_file_tag));
    nuevo->file = strdup(file);
    nuevo->tag  = strdup(tag);
    nuevo->tabla_de_paginas = list_create();

    list_add(proceso_mem->files_tags, nuevo);
}

char* solicitar_pagina_storage(int pid, char* file, char* tag, int nro_pagina) {

    t_buffer* buffer = crear_buffer();
    cargar_string_al_buffer(buffer, file);
    cargar_string_al_buffer(buffer, tag);
    cargar_entero_al_buffer(buffer, nro_pagina);
    cargar_entero_al_buffer(buffer, pid);

    t_paquete* paquete = crear_paquete(SOL_READ, buffer);

    enviar_paquete(paquete, fd_storage);
    eliminar_paquete(paquete);

    int codigo = recibir_operacion(fd_storage);

    switch (codigo) {
        case RTA_READ: {

            t_buffer* resp = recibir_todo_un_buffer(fd_storage);
            char* contenido = extraer_string_del_buffer(resp);
            eliminar_buffer(resp);

            return contenido;
        }

        case FILE_INEXISTENTE:
        case LECTURA_FUERA_DE_LIMITE:
        case ESPACIO_INSUFICIENTE:
        {
            t_buffer* buffer_rta = recibir_todo_un_buffer(fd_storage);
            char* motivo = strdup(extraer_string_del_buffer(buffer_rta));
            eliminar_buffer(buffer_rta);

            t_buffer* buffer_fin = crear_buffer();
            cargar_entero_al_buffer(buffer_fin, pid);
            cargar_string_al_buffer(buffer_fin, motivo);

            liberar_proceso();

            t_paquete* paquete_fin = crear_paquete(FIN_POR_ERROR, buffer_fin);
            enviar_paquete(paquete_fin, fd_master);
            eliminar_paquete(paquete_fin);

            free(motivo);
            return NULL;
        }

        default:
            exit(EXIT_FAILURE);
    }
}
