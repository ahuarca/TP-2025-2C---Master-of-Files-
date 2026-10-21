#include "operaciones.h"

#include <errno.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/crypto.h>
#include <sys/stat.h>
#include <utils/utils.h>
#include "../filesystem/util.h"
#include "../filesystem/operaciones_fs.h"
#include "../main.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>


int create_file(char* file_name, char* tag_name, int pid){

    t_file* file_preexistente =  dictionary_get(dict_files, file_name);
   
    if(file_preexistente != NULL){
        t_tag* tag_preexistente = dictionary_get(file_preexistente->dict_tags,tag_name);
        if(tag_preexistente!=NULL){
            return -1; //CONTROLADO-> FILE:TAG QUE YA EXISTE
        }
    }

    t_file* file;
    t_tag* tag;
    int error = 0;

    if(file_preexistente == NULL){

        file = malloc(sizeof(t_file));
        error = crear_estructura_file(file, file_name, pid,tag_name);
    }else{
        file = file_preexistente;
    }

    tag = malloc(sizeof(t_tag));

    if (error == -3){
        log_error(logger, "Error creando file, file exists");
        return -2;
    }
    if (error != 0)
    {
        log_error(logger, "Error creando file");
        return -2;
    }
    error = crear_estructura_tag(tag, file_name, tag_name, pid);

    if (error != 0)
    {
        log_error(logger, "Error creando file");
        return -2;
    }

    pthread_mutex_lock(&file->mutexDictTags);
    dictionary_put(file->dict_tags, tag->nombre_tag, tag);
    pthread_mutex_unlock(&file->mutexDictTags);


    if(file_preexistente == NULL){

        pthread_mutex_lock(&mutexDirectorioFiles); 
        dictionary_put(dict_files, file->name, file);
        pthread_mutex_unlock(&mutexDirectorioFiles);
    }
  
    return 0;
}

int truncar_file(char* file_name, char* tag_name, int nuevo_tamanio, int pid)
{
    int cant_bloques = nuevo_tamanio / super_config->block_size;

    if (cant_bloques < 0)
    {
        log_error(logger, "Error validacion, cant_bloques es negativo");
        return -2;
    }
    pthread_mutex_lock(&mutexDirectorioFiles);
    t_file* file = dictionary_get(dict_files, file_name);
    pthread_mutex_unlock(&mutexDirectorioFiles);

    if (file == NULL)
    {
        log_error(logger, "Error getting file");
        return -1;//-> CONTROLADO
    }

    pthread_mutex_lock(&file->mutexDictTags);
    t_tag* tag = dictionary_get(file->dict_tags, tag_name);
    pthread_mutex_unlock(&file->mutexDictTags);

    if (tag == NULL)
    {
        log_error(logger, "Error getting file");
        return -1; //-> CONTROLADO
    }

    char* estadoTag = config_get_string_value(tag->metadata_config, "ESTADO");

        if(strcmp(estadoTag,METADATA_CONFIG_STATUS_COMMITED)==0){
            return -9;
        }

    pthread_mutex_lock(&tag->mutexListaBloquesTag);
    int tamaño_actual = list_size(tag->lista_bloques);
    pthread_mutex_unlock(&tag->mutexListaBloquesTag);
    //  -3    =  2  - 5

    int cant_bloques_a_reservar = tamaño_actual - cant_bloques;

    if (cant_bloques_a_reservar < 0) //-3 < 0
    {
        cant_bloques_a_reservar = -cant_bloques_a_reservar; // Valor absoluto -> 3
        
        // Hay que agregarle bloques
        int* bloques = malloc(sizeof(int) * cant_bloques_a_reservar);  //vector de 3 lugares
        int error = reservar_bloques_por_primera_vez(bloques, cant_bloques_a_reservar, pid);

        if (error < 0)
        {
            if (error == -2)
            {
                log_info(logger, "No hay bloques disponibles, el tag no puedo reservar ni un solo bloque");
                return -10; //-> CONTROLADO
            }
            log_info(logger, "Error asignando bloques por algún motivo inesperado");
            return -3; // -> CONTROLADO
        }
        for (int i = 0; i < cant_bloques_a_reservar; i++)
        {
            if (add_block_to_tag(file_name, tag_name, bloques[i], -1, pid) < 0)
            {
                log_info(logger, "Error agregando bloque a tag");
                exit(EXIT_FAILURE);
            }
   
        }

        aumentar_size_en_metadata_config(tag, cant_bloques_a_reservar);
    }
    else
    {
        // Hay que sacarle bloques
        for (int i = 0; i < cant_bloques_a_reservar; i++)
        {
            pthread_mutex_lock(&tag->mutexListaBloquesTag);
            int ultimo_bloque_index = list_size(tag->lista_bloques) - 1;   
            int bloque_a_eliminar = *(int*)list_get(tag->lista_bloques, ultimo_bloque_index);
            pthread_mutex_unlock(&tag->mutexListaBloquesTag);

            if (eliminar_bloque_logico(file_name, tag_name, bloque_a_eliminar, ultimo_bloque_index, pid) < 0)
            {
                if (i == 0)
                {
                    log_error(logger, "Error eliminando  el ultimo bloque de tag");
                    return -4;
                }
                log_error(logger, "Error eliminando bloque de tag, estado irrecuperable");
                exit(EXIT_FAILURE);
            }

            disminuir_bloques_en_metadata_config(tag, bloque_a_eliminar,ultimo_bloque_index );

            pthread_mutex_lock(&tag->mutexListaBloquesTag);
            int* numBloquePtr = list_remove(tag->lista_bloques, ultimo_bloque_index);
            pthread_mutex_unlock(&tag->mutexListaBloquesTag);
            
            free(numBloquePtr);
        }

        disminuir_size_en_metadata_config(tag, cant_bloques_a_reservar);
       
       
    }
    return 0;
}

int copiar_tag(char* file_name_src, char* tag_name_src, char* file_name_dst,char* nombre_del_nuevo_tag ,int pid){

   
    t_file* file_origen = dictionary_get(dict_files, file_name_src);
 
    if (file_origen == NULL)
    {
        log_error(logger, "Error getting file");
        return -1;
    }

    t_tag* tag_preexistente = dictionary_get(file_origen->dict_tags,nombre_del_nuevo_tag);
        if(tag_preexistente!=NULL){
            return -9; //SIGNIFICA QUE EL FILE:TAG YA EXISTE 
        }

    t_file* file_destino = dictionary_get(dict_files, file_name_dst);
   

    if (file_destino == NULL) // por si el directorio del file destino no existe
    {
        t_file* file = malloc(sizeof(t_file));
        pthread_mutex_init(&file->mutexDictTags, NULL);
        int rta = crear_estructura_file(file, file_name_dst, pid, nombre_del_nuevo_tag);
            if(rta < 0){
                return rta;
            }
    }
    
    t_tag* tag_src = dictionary_get(file_origen->dict_tags, tag_name_src);
    

    if (tag_src == NULL)
    {
        log_error(logger, "Error getting tag");
        return -1;
    }

    t_tag* tag_dst = malloc(sizeof(t_tag));
    int error = crear_estructura_tag(tag_dst,file_name_dst,nombre_del_nuevo_tag, pid);

    if (error != 0)
    {
        log_error(logger, "Error creando tag");
        return -2;
    }

    // Por ultimo agregar el tag a la estructura file
    pthread_mutex_lock(&file_destino->mutexDictTags);
    dictionary_put(file_destino->dict_tags, tag_dst->nombre_tag, tag_dst);
    pthread_mutex_unlock(&file_destino->mutexDictTags);

    // Copiar todos los bloques del tag
    bool loop_error = false;
    for (int i = 0; i < list_size(tag_src->lista_bloques); i++)
    {
        // Agregar bloque a la lista
        int* block_num_ptr = malloc(sizeof(int));

        if (block_num_ptr == NULL)
        {
            log_error(logger, "Error getting block_num_ptr");
            loop_error = true;
            break;
        }
        pthread_mutex_lock(&tag_src->mutexListaBloquesTag);
        *block_num_ptr = *(int*)list_get(tag_src->lista_bloques, i);
        pthread_mutex_unlock(&tag_src->mutexListaBloquesTag);

        // Copiar bloques modificando metadata config
        if (add_block_to_tag(file_name_dst, nombre_del_nuevo_tag, *block_num_ptr, -1, pid) < 0)
        {
            log_error(logger, "Error adding block to tag");
            loop_error = true;
            break;
        }
    }

    int cantidad_bloques_copiados = list_size(tag_src->lista_bloques);
    aumentar_size_en_metadata_config(tag_dst,cantidad_bloques_copiados);

    if (loop_error)
    {
        // Undo partial changes
        char* path = malloc(sizeof(char) * PATH_MAX);
        if (get_path_by_file_name_and_tag(path, file_name_dst, nombre_del_nuevo_tag) < 0)
        {
            log_error(logger, "Error getting path, undo partial changes");
            exit(EXIT_FAILURE);
        }
        if (eliminar_archivos_de_directorio(path) < 0)
        {
            log_error(logger, "Error deleting tag, undo partial changes");
            exit(EXIT_FAILURE);
        }
        return -3;
    }
  

    return 0;
}

int commit_tag(char* file_name, char* tag_name, int pid)
{
    pthread_mutex_lock(&mutexDirectorioFiles);
    t_file* file = dictionary_get(dict_files, file_name);
    pthread_mutex_unlock(&mutexDirectorioFiles);

    if (file == NULL)
    {
        log_error(logger, "Error getting file");
        return -1;
    }
    pthread_mutex_lock(&file->mutexDictTags);
    t_tag* tag = dictionary_get(file->dict_tags, tag_name);
    pthread_mutex_unlock(&file->mutexDictTags);

    if (tag == NULL)
    {
        log_error(logger, "Error getting tag");
        return -1;
    }
    pthread_mutex_lock(&tag->mutexMetadataTag);
    char* status = config_get_string_value(tag->metadata_config, METADATA_CONFIG_STATUS);
    pthread_mutex_unlock(&tag->mutexMetadataTag);

    if (status == NULL)
    {
        log_error(logger, "Error leyendo etado de la config del Tag");
        return -6;
    }
    if (strcmp(status, METADATA_CONFIG_STATUS_COMMITED) == 0)
    {   
        log_warning(logger, "No podes commitear este tag porque ya fue commiteado");
        return -5; // -> CONTROLADO
    }

    if (update_tag_status(file_name, tag_name, METADATA_CONFIG_STATUS_COMMITED) < 0)
    {
        return -4; 
    }

    for (int i = 0; i < list_size(tag->lista_bloques); i++)
    {
        pthread_mutex_lock(&tag->mutexListaBloquesTag);
        int* num_bloque_fisico = list_get(tag->lista_bloques, i);
        pthread_mutex_unlock(&tag->mutexListaBloquesTag);

        int num_BF = *num_bloque_fisico;
        char* nombreDelBloqueEnConfig = NULL;
        char* hash = hash_bloque(num_BF);

        if (hash == NULL){
            log_error(logger, "Error creando hash del bloque");
            exit(EXIT_FAILURE);
        }

        if(config_keys_amount(config_hash) == 0 ){

            if (set_block_hash_in_map(num_BF, hash) < 0)
            {
                log_error(logger, "Error guardando hash del bloque");
                free(hash);
                exit(EXIT_FAILURE);
            }

            free(hash);

        }else{
            if(config_has_property(config_hash, hash)){

                pthread_mutex_lock(&mutexConfigHash);
                nombreDelBloqueEnConfig = config_get_string_value(config_hash, hash);
                pthread_mutex_unlock(&mutexConfigHash);

                int nuevo_BF_a_reapuntar = buscarBloqueFisicoPorNombreEnConfigHash(nombreDelBloqueEnConfig);

                if (eliminar_bloque_logico(file_name, tag_name, *num_bloque_fisico, i, pid) < 0)
                {
                    log_error(logger, "Error eliminando bloque logico");
                    free(hash);
                    exit(EXIT_FAILURE);
                }
                
                pthread_mutex_lock(&tag->mutexListaBloquesTag);
                int* num_BF_ptr =list_remove(tag->lista_bloques, i);
                pthread_mutex_unlock(&tag->mutexListaBloquesTag);

                free(num_BF_ptr);

                if (add_block_to_tag_en_write(file_name,tag_name,nuevo_BF_a_reapuntar,i,pid)< 0){
                    log_error(logger, "Error adding block to tag for deduplication");
                    free(hash);
                    exit(EXIT_FAILURE);
                }

                free(hash);
                //log obligatorio
                log_info(logger, "Deduplicación de Bloque: ##<QUERY_ID><%d> - <%s>:<%s> Bloque Lógico <%d> se reasigna de <%d> a <%d>",pid,file_name,tag_name,i,num_BF,nuevo_BF_a_reapuntar);
            }else{
                if (set_block_hash_in_map(*num_bloque_fisico , hash) < 0)
                {
                    log_error(logger, "Error guardando hash del bloque");
                    free(hash);
                    exit(EXIT_FAILURE);
                }
                free(hash);
            }
        }
     
        
    }
    return 0;
}

int delete_tag(char* file_name, char* tag_name, int pid)
{
    pthread_mutex_lock(&mutexDirectorioFiles);
    t_file* file = dictionary_get(dict_files, file_name);
    pthread_mutex_unlock(&mutexDirectorioFiles);

    if (file == NULL)
    {
        log_error(logger, "Error finding file");
        return -1;
    }
    pthread_mutex_lock(&file->mutexDictTags);
    t_tag* tag = dictionary_get(file->dict_tags, tag_name);
    pthread_mutex_unlock(&file->mutexDictTags);

    if (tag == NULL)
    {
        log_error(logger, "Error finding tag");
        return -2;
    }
    for (int i = 0; i < list_size(tag->lista_bloques); i++)
    {
        pthread_mutex_lock(&tag->mutexListaBloquesTag);
        int* block_num_ptr = list_get(tag->lista_bloques, i);
        pthread_mutex_unlock(&tag->mutexListaBloquesTag);

        if (block_num_ptr == NULL)
        {
            log_error(logger, "Error getting block_num_ptr");
            exit(EXIT_FAILURE);
        }
        if (eliminar_bloque_logico(file_name, tag_name, *block_num_ptr, i, pid) < 0)
        {
            log_error(logger, "Error deleting logial block");
            exit(EXIT_FAILURE);
        }

        if(quitarHashDeLaConfigDeHashes(*block_num_ptr) < 0){
           log_debug(logger, "no se rmeuve de la config de hashes el bloque fisico %d porque esta duplicado o no estaba desde un principio",*block_num_ptr);
        }
    }
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (path == NULL)
    {
        log_error(logger, "Error allocating path");
        exit(EXIT_FAILURE);
    }
    if (get_path_by_file_name_and_tag(path, file_name, tag_name) < 0)
    {
        log_error(logger, "Error getting tag path");
        exit(EXIT_FAILURE);
    }
    if (eliminar_archivos_de_directorio(path) < 0){
        log_error(logger, "Error deleting tag dir");
        exit(EXIT_FAILURE);
    }
    t_tag* tag_a_eliminar = dictionary_remove(file->dict_tags, tag_name);

    liberar_tag(tag_a_eliminar);

    free(path);
    return 0;
}
int quitarHashDeLaConfigDeHashes(int num_bloque_fisico){
   
    char* hash = hash_bloque(num_bloque_fisico);

    if(config_has_property (config_hash, hash) && !bloque_duplicado(num_bloque_fisico)){

        pthread_mutex_lock(&mutexConfigHash);
        config_remove_key(config_hash, hash);
        int error = config_save(config_hash);
        pthread_mutex_unlock(&mutexConfigHash);

        if (error < 0){
            log_error(logger, "Error removiendo hash de la config de hashes");
            free(hash);
            return -2;
        }
        log_debug(logger, "El bloque fisico %d se removio de la config de hashes",num_bloque_fisico);
        free(hash);
        return 0;
    }
    
    free(hash);
    return -1;// CONTROLADO
    
}

/**
 *
 * @return Retorna -2 si el tag no contiene al bloque, < 0 por error, 0 si la operacion es exitosa
 */
int leer_bloque(char** out_content, char* file_name, char* tag_name, int num_bloque_logico)
{
    pthread_mutex_lock(&mutexDirectorioFiles);
    t_file* file = dictionary_get(dict_files, file_name);
    pthread_mutex_unlock(&mutexDirectorioFiles);

    if (file == NULL)
    {
        log_error(logger, "Error getting file");
        return -1;
    }
    pthread_mutex_lock(&file->mutexDictTags);
    t_tag* tag = dictionary_get(file->dict_tags, tag_name);
    pthread_mutex_unlock(&file->mutexDictTags);

    if (tag == NULL)
    {
        log_error(logger, "Error getting tag");
        return -1;
    }

    if(list_size(tag->lista_bloques)<= num_bloque_logico){
        log_warning(logger, "Se esta intentando leer fuera del limite porque ese bloque logico no pertence a la lista de bloques logicos del Tag");
        return -9;
    }

    //el num_bloque_logico es el indice en la lista de bloques fisicoa del tag
    pthread_mutex_lock(&tag->mutexListaBloquesTag);
    int num_bloque_fisico = *(int*)list_get(tag->lista_bloques, num_bloque_logico);
    pthread_mutex_unlock(&tag->mutexListaBloquesTag);

  

    if (read_block(out_content, num_bloque_fisico) < 0)
    {
        log_error(logger, "Error leyendo bloque");
        return -3;
    }
    return 0;
}





int escribir_bloque(char* file_name, char* tag_name, int logical_block_number, char* content, int pid)
{
    pthread_mutex_lock(&mutexDirectorioFiles);
    // Checkear estado tag
    t_file* file = dictionary_get(dict_files, file_name);
    pthread_mutex_unlock(&mutexDirectorioFiles);

    if (file == NULL)
    {
        log_error(logger, "Error getting file");
        return -1;
    }
    pthread_mutex_lock(&file->mutexDictTags);
    t_tag* tag = dictionary_get(file->dict_tags, tag_name);
    pthread_mutex_unlock(&file->mutexDictTags);

    if (tag == NULL)
    {
        log_error(logger, "Error getting tag");
        return -1;
    }
    pthread_mutex_lock(&tag->mutexMetadataTag);
    char* status = config_get_string_value(tag->metadata_config, METADATA_CONFIG_STATUS);
    pthread_mutex_unlock(&tag->mutexMetadataTag);

    if (status == NULL)
    {
        log_error(logger, "Error leyendo etado de la config del Tag");
        return -6;
    }
    if (strcmp(status, METADATA_CONFIG_STATUS_COMMITED) == 0)
    {   
        log_warning(logger, "No podes escribir en este tag porque ya fue commiteado");
        return -3;
    }

 
    if (list_size(tag->lista_bloques) < logical_block_number)
    {
        log_error(logger, "No existe el bloque logico solicitado %d", logical_block_number);
        return -5;
    }

     // Buscar si es una copia

    pthread_mutex_lock(&tag->mutexListaBloquesTag);
    int* physical_block_num = list_get(tag->lista_bloques, logical_block_number);
    pthread_mutex_unlock(&tag->mutexListaBloquesTag);

    
    int cantHardLinks = obtenerCantHardLinks(*physical_block_num);

        if(cantHardLinks < 0){
            log_error(logger, "la cantidad de HL es negativa");
            exit(1);
        }

    if (cantHardLinks >= 2)
    {
        // Escribir en otro bloque
        if (eliminar_bloque_logico(file_name, tag_name, *physical_block_num, logical_block_number, pid) < 0){
            log_error(logger, "Error eliminando bloque logico para escribir en nuevo bloque");
            return -6;
        }
       
        pthread_mutex_lock(&tag->mutexListaBloquesTag);
        int* bloque_fisico_ptr = list_remove(tag->lista_bloques, logical_block_number);
        pthread_mutex_unlock(&tag->mutexListaBloquesTag);

        free(bloque_fisico_ptr);

        int* bloques = malloc(sizeof(int) * 1);

        if (reservar_bloques(bloques, 1, pid) < 0)
        {
            log_error(logger, "Error reservando bloques para escribir en nuevo bloque");
            return -2;
        }

        if (add_block_to_tag_en_write(file_name, tag_name, bloques[0],logical_block_number, pid) < 0)
        {
            log_error(logger, "Error agregando nuevo bloque para escribir en nuevo bloque");
            exit(EXIT_FAILURE);
        }
        if (write_to_block(file_name, tag_name, logical_block_number, bloques[0], content) < 0){
            log_error(logger, "Error escribiendo bloque: num fisico = %d, num logico = %d",
                bloques[0], logical_block_number);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Escribir en este bloque
        if (write_to_block(file_name, tag_name, logical_block_number, *physical_block_num, content) < 0){
            log_error(logger, "Error escribiendo bloque: num fisico = %d, num logico = %d",
                *physical_block_num, logical_block_number);
            return -7;
        }
    }
    return 0;
}

//aca no pongo mutex porque no lo usamos
void flush_file(char* nombre_file, char* nombre_tag, char* contenido_a_persistir, int num_bloque_logico){

    t_file* file = dictionary_get(dict_files, nombre_file);

    if (file == NULL)
    {
        log_error(logger, "No se encontro el file en el dict_files, en la operacion flush_file");
        return;
    }
    t_tag* tag = dictionary_get(file->dict_tags, nombre_tag);

    if (tag == NULL)
    {
       log_error(logger, "No se encontro el tag, en la operacion flush_file");
       
        return;
    }

    int num_bloque_fisico = *(int*)list_get(tag->lista_bloques, num_bloque_logico);

    int rta = write_to_block(nombre_file, nombre_tag, num_bloque_logico, num_bloque_fisico, contenido_a_persistir);

    if(rta<0){
        log_error(logger, "Error escribiendo bloque en la operacion flush_file");
        return;
    }

    
}

int buscarBloqueFisicoPorNombreEnConfigHash(char* nombreDelBloque){

    for(int i =0; i<list_size(lista_bloques_fisicos); i++){
        t_bloque_fisico* bloque = list_get(lista_bloques_fisicos, i);
        if(strcmp(bloque->nombre_bloque_fisico, nombreDelBloque) == 0){
            return i;
        }

    }
    return -1;
}


void rellenar_diccionarios(const char*  path_base){

    dir_lista_t lista = restaurar_files_tags(path_base);

    for(int i = 0; i < lista.cantidad; i++){

        t_file* filePreexistente = dictionary_get(dict_files, lista.array_paths[i].nombre_directorio);

        if(filePreexistente != NULL){

            t_tag* tag = malloc(sizeof(t_tag));
            tag->nombre_tag = lista.array_paths[i].nombre_tag;
            tag->lista_bloques = list_create();
    
            char rutaMetadataTag[200] = {0};
            snprintf(rutaMetadataTag, sizeof(rutaMetadataTag),"%s/%s/%s/metadata.config",path_base,lista.array_paths[i].nombre_directorio,lista.array_paths[i].nombre_tag);
            
            tag->metadata_config = config_create(rutaMetadataTag);
    
            pthread_mutex_init(&tag->mutexListaBloquesTag,NULL);
            pthread_mutex_init(&tag->mutexMetadataTag,NULL);
    
            agregarBloquesTagSegunMetadata(tag);

            dictionary_put(filePreexistente->dict_tags,tag->nombre_tag,tag);
    
        }else 
        {
            t_file* file = malloc(sizeof(t_file));
            file->dict_tags = dictionary_create();
            pthread_mutex_init(&file->mutexDictTags, NULL);
            file->name = lista.array_paths[i].nombre_directorio;
            file->path = lista.array_paths[i].path_completo;
            
            t_tag* tag = malloc(sizeof(t_tag));
            tag->nombre_tag = lista.array_paths[i].nombre_tag;
            tag->lista_bloques = list_create();
    
            char rutaMetadataTag[200] = {0};
            snprintf(rutaMetadataTag, sizeof(rutaMetadataTag),"%s/%s/%s/metadata.config",path_base,lista.array_paths[i].nombre_directorio,lista.array_paths[i].nombre_tag);
            
            tag->metadata_config = config_create(rutaMetadataTag);
    
            pthread_mutex_init(&tag->mutexListaBloquesTag,NULL);
            pthread_mutex_init(&tag->mutexMetadataTag,NULL);
    
            agregarBloquesTagSegunMetadata(tag);
    
            dictionary_put(file->dict_tags,tag->nombre_tag ,tag);
            dictionary_put(dict_files,file->name,file);
    
        }
        
        
      
    }


}

dir_lista_t restaurar_files_tags(const char* path_base) {

    DIR* dir = opendir(path_base);
    struct dirent* entry;

    dir_lista_t result = { .array_paths = NULL, .cantidad = 0 };

    if (!dir) {
        perror("opendir");
        return result;
    }

    while ((entry = readdir(dir)) != NULL) {

        // Ignorar . y ..
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        // Nombre directorio de 1er nivel (ej: initial_file)
        const char* nombre_directorio = entry->d_name;

        // Path hacia el file 
        char dir_path[PATH_MAX];
        memset(dir_path, 0, sizeof(dir_path));
        snprintf(dir_path, sizeof(dir_path), "%s/%s", path_base, nombre_directorio);

        // Verificar que sea directorio
        struct stat st;
        if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)){

            DIR* subdir = opendir(dir_path);
            if (!subdir) continue;

            struct dirent* sub_entry;

            //recorro las entradas del file, busco sus tags 
            while ((sub_entry = readdir(subdir)) != NULL) {

                if (!strcmp(sub_entry->d_name, ".") || !strcmp(sub_entry->d_name, ".."))
                    continue;

                //obtengo el path del TAG
                char tag_path[PATH_MAX] = {0};
                snprintf(tag_path, sizeof(tag_path), "%s/%s", dir_path, sub_entry->d_name);

                struct stat st2;
                if (stat(tag_path, &st2) == 0 && S_ISDIR(st2.st_mode)) {

                    // Realloc seguro
                    file_tag_info_t* tmp = realloc(result.array_paths, sizeof(file_tag_info_t) * (result.cantidad + 1));

                    if (!tmp) {
                        perror("realloc");
                        closedir(subdir);
                        closedir(dir);
                        return result;
                    }

                    result.array_paths = tmp;

                    // Guardar información
                    result.array_paths[result.cantidad].path_completo     = strdup(dir_path);
                    result.array_paths[result.cantidad].nombre_directorio = strdup(nombre_directorio);
                    result.array_paths[result.cantidad].nombre_tag        = strdup(sub_entry->d_name);

                    result.cantidad++;
                }
            }

            closedir(subdir);
        }
    }

    closedir(dir);
    return result;
}

void agregarBloquesTagSegunMetadata(t_tag* tag){

    char** blocks = config_get_array_value(tag->metadata_config, METADATA_CONFIG_BLOCKS);
    if (blocks == NULL)
    {
        log_error(logger, "Error obteniendo el array de bloques de la metadata");
        return;
    }

    for(int i = 0; i< string_array_size(blocks); i++){
        int* numBF = malloc(sizeof(int));
        *numBF = atoi(blocks[i]); 
        list_add(tag->lista_bloques, numBF); 
    }

}
