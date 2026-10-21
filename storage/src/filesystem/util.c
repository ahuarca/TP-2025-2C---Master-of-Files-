#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <commons/crypto.h>
#include <commons/log.h>
#include <dirent.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <utils/utils.h>

#include "../main.h"



int get_path_by_file_name(char* path, char* file_name)
{
    if (build_path(
        path, PATH_MAX,
        configuration->punto_montaje, NOMBRE_FILE_DIR, file_name, NULL
        ) < 0)
    {
        perror("Error creando file, build_path failed");
        return -1;
    }
    return 0;
}

int get_path_by_file_name_and_tag(char* path, char* file_name, char* tag_name)
{
    if (build_path(
        path, PATH_MAX,
        configuration->punto_montaje, NOMBRE_FILE_DIR, file_name, tag_name, NULL
    ) < 0)
    {
        perror("Error creando file, build_path failed");
        return -1;
    }
    return 0;
}

int get_logical_blocks_path_by_file_name_and_tag_name(char* path, char* file_name, char* tag_name)
{
    if (build_path(
    path, PATH_MAX,
    configuration->punto_montaje, NOMBRE_FILE_DIR, file_name, tag_name, LOGICAL_BLOCKS_DIRNAME, NULL
    ) < 0)
    {
        perror("Error creando tag, build_path failed");
        return -1;
    }
    return 0;
}

int get_metadata_config_path_by_file_name_and_tag_name(char* path, char* file_name, char* tag_name)
{
    if (build_path(
    path, PATH_MAX,
    configuration->punto_montaje, NOMBRE_FILE_DIR, file_name, tag_name, METADATA_CONFIG_NAME, NULL
    ) < 0)
    {
        perror("Error creando el path para la config metadata del tag");
        return -1;
    }
    return 0;
}

int get_logical_block_path(char* path, char* file_name, char* tag_name, const int num_bloque_logico)
{
    
    char nombreBloqueLogico [20];
    snprintf(nombreBloqueLogico, sizeof(nombreBloqueLogico), "%06d", num_bloque_logico);
    strcat(nombreBloqueLogico, ".dat");

    if (nombreBloqueLogico == NULL)
    {
        perror("Error creando path logical block, block_num_to_block_name");
        return -1;
    }
 
    if (build_path(
    path, PATH_MAX,
    configuration->punto_montaje, NOMBRE_FILE_DIR,
    file_name, tag_name, LOGICAL_BLOCKS_DIRNAME,
    nombreBloqueLogico,NULL) < 0)
    {
        perror("Error creando path logical block, build_path failed");
        resetear_cstring(nombreBloqueLogico);
        return -1;
    }
    resetear_cstring(nombreBloqueLogico);
    return 0;
}

char *join_strings(char **arr, const size_t count) {
    if (count == 0) {
        char *empty = malloc(3);
        if (!empty) 
        return NULL;
        strcpy(empty, "[]");
        return empty;
    }

    size_t total = 2;
    for (size_t i = 0; i < count; i++) {
        total += strlen(arr[i]);
        if (i < count - 1)
            total += 1; // comma
    }
    total += 1; // null terminator

    char *result = malloc(total);
    if (!result) return NULL;

    char *p = result;
    *p++ = '[';

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(arr[i]);
        memcpy(p, arr[i], len);
        p += len;
        if (i < count - 1)
            *p++ = ',';
    }

    *p++ = ']';
    *p = '\0';

    return result;
}

int crear_directorio(char* path){
    if (mkdir(path, 0777) == -1) {
        perror("Creando directorio");
        if (errno == EEXIST) {
            log_error(logger, "El directorio del file ya existe"); //esto no deberia pasar nunca segun el enunciado
            return -1;
        }
        log_error(logger, "Ocurrió un error al crear el directorio");
        return -2;
    }
    log_debug(logger,"Se creo exitosamente el directorio del file %s!", path);
    return 0;
}

int generar_hard_link(char* ruta_del_origen, char* ruta_destino){
    if(link(ruta_del_origen, ruta_destino) < 0){
        log_error(logger, "No se pudo crear el hard link  de %s en %s", ruta_del_origen, ruta_destino);
        return -1;
    }
    log_debug(logger, "Se creo exitosamente el hard link  de %s en %s", ruta_del_origen, ruta_destino);
    return 0;
}

/**
 *
 * @param block_number
 * @return 0 if false, 1 if true, -1 if the block does not exist in the config file
 */
int bloque_duplicado(const int block_number)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque_fisico = list_get(lista_bloques_fisicos, block_number);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque_fisico == NULL)
    {
        return -1;
    }
    pthread_mutex_lock(&bloque_fisico->mutex_BF);
    if (obtenerCantHardLinks(block_number) > 2)
    {
        pthread_mutex_unlock(&bloque_fisico->mutex_BF);
        return 1;
    }
    pthread_mutex_unlock(&bloque_fisico->mutex_BF);

    return 0; //si no es mayor a 2, entonces o es 2 (sólo hay un HL) o 1 (No hay n ningun HL, pero esto no es porbable que pase), en ese caso no esta duplicado.
}

//con que tenga un sólo harlink ya es suficiente para marcarlo como ocupado en el bitmap
int cant_hardlinks_de_bloque(const int block_number)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque_fisico = list_get(lista_bloques_fisicos, block_number);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque_fisico == NULL)
    {
        return -1;
    }

    struct stat st;

    if (stat(bloque_fisico->ruta_del_bloque_fisico, &st) == -1) {
        log_error(logger, "Error leyendo la ruta del bloque físico con stat");
        return -1;
    }


    if(st.st_nlink == 1 ){ // lo referencia solo el directorio padre, porque el archivo siempre tiene un hardlink de parte de su directorio padre y el otro ya si es de otro archivo
        return 0; // true para la llamada 
    }
    if(st.st_nlink >2){   // hay mas d eun archivo referencioandolo
        return 1; // false para la llamada
    }

    return -1;
}

int obtenerCantHardLinks(const int block_number)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque_fisico = list_get(lista_bloques_fisicos, block_number);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque_fisico == NULL)
    {
        return -1;
    }

    struct stat st;

    if (stat(bloque_fisico->ruta_del_bloque_fisico, &st) == -1) {
        log_error(logger, "Error leyendo la ruta del bloque físico con stat");
        return -1;
    }

    log_debug(logger, "El archivo %s tiene %d HL",bloque_fisico->ruta_del_bloque_fisico, st.st_nlink);
    return st.st_nlink; 
    
}

int restar_inode_de_bloque(const int block_number)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque_fisico = list_get(lista_bloques_fisicos, block_number);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque_fisico == NULL)
    {
        return -1;
    }
    if (bloque_fisico->inode_cant > 0) {

        pthread_mutex_lock(&bloque_fisico->mutex_BF);
        bloque_fisico->inode_cant = bloque_fisico->inode_cant - 1;
        pthread_mutex_unlock(&bloque_fisico->mutex_BF);
        return 0;
    }
       
    return -1;
}

int sumar_inode_de_bloque(const int block_number)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque_fisico = list_get(lista_bloques_fisicos, block_number);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque_fisico == NULL)
    {
        return -1;
    }
    pthread_mutex_lock(&bloque_fisico->mutex_BF);
    bloque_fisico->inode_cant = bloque_fisico->inode_cant + 1;
    pthread_mutex_unlock(&bloque_fisico->mutex_BF);
    return 0;
}

int set_block_hash_in_map(int block_number, char* hash)
{
    pthread_mutex_lock(&mutexConfigHash);
    config_set_value(config_hash, hash, block_num_to_block_name(block_number));
    if (config_save(config_hash) < 0){
        pthread_mutex_unlock(&mutexConfigHash);
        log_error(logger, "Error saving block hash config");
        return -1;
    }
    pthread_mutex_unlock(&mutexConfigHash);
    return 0;
}

char* hash_bloque(const int num_bloque){
    char* content = NULL;

    if (read_block(&content, num_bloque) < 0)
    {
        log_error(logger, "Error leyendo bloque");
        return NULL;
    }
    return crypto_md5(content, super_config->block_size);
}

int read_block(char** out, const int num_bloque)
{
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque = list_get(lista_bloques_fisicos, num_bloque);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque == NULL)
    {
        log_error(logger, "Error obteniendo bloque de lista nro %d", num_bloque);
        return -1;
    }

    int fd = open(bloque->ruta_del_bloque_fisico, O_RDONLY);
    void* temp_map = mmap(NULL, super_config->block_size, PROT_READ, MAP_SHARED, fd, 0);

    close(fd);

    *out = malloc(super_config->block_size + 1);
    memcpy(*out, temp_map, super_config->block_size);
    
    (*out)[super_config->block_size] = '\0';

    munmap(temp_map, super_config->block_size);

    // pthread_mutex_lock(&bloque->mutexParaLeerOEscribir);
    
    // FILE* file = fopen(bloque->ruta_del_bloque_fisico, "r");

    // if (file == NULL)
    // {
    //     log_error(logger, "Error abriendo bloque");
    //     pthread_mutex_unlock(&bloque->mutexParaLeerOEscribir);
    //     return -2;
    // }
    // *out = read_all(file);

    // fclose(file);

    // pthread_mutex_unlock(&bloque->mutexParaLeerOEscribir);

    // if (*out == NULL)
    // {
    //     log_error(logger, "Error leyendo bloque");
    //     return -3;
    // }
    
    return 0;
}

char* read_all(FILE* f) {
  
    size_t tam_bloque = super_config->block_size;

    // Reservo tam_bloque bytes
    char* buf = malloc( tam_bloque + 1);
    if (!buf) return NULL;

    // Leo exactamente tam_bloque bytes
    size_t n = fread(buf, 1, tam_bloque, f);

    buf[tam_bloque] = '\0';

    return buf;
    
}

bool directorio_existe(char *ruta_de_directorio){
    struct stat st;
    return stat(ruta_de_directorio, &st) == 0 && S_ISDIR(st.st_mode);
}

char* block_num_to_block_name(const int num)
{
    char *name = malloc(20);
    if (!name) return NULL;
    snprintf(name, 20, "block%04d", num);
    return name;
}

int eliminar_archivos_de_directorio(char* ruta_dir){
    struct dirent *entrada;
    DIR *dir = opendir(ruta_dir);

    if (!dir) return 0;
    while ((entrada = readdir(dir)) != NULL) {
        char path[1024];
        // Ignorar . (dir actual) y .. (dir padre)
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0){
           continue;
        }

        snprintf(path, sizeof(path), "%s/%s", ruta_dir, entrada->d_name);

        struct stat st;
        if (stat(path, &st) == 0)
        {
            //comprueba si el path corresponde a un archivo o directorio
            if (S_ISDIR(st.st_mode)) {
                const int err = eliminar_archivos_de_directorio(path);  //si es un dir, vuelve a llamar a la funcion para que se elimenen todos los archivos dentro de ese dir

                if (err < 0)
                {
                    return err;
                }
            }
            else // elimina si el path es un archivoo
            {
                if (remove(path) < 0)
                {
                    log_error(logger, "Error eliminando archivo");
                    return -1;
                }
            }
        }
    }
    closedir(dir);
    rmdir(ruta_dir);
    return 0;
}

void liberar_tag(t_tag* tag_a_eliminar){

    free(tag_a_eliminar->nombre_tag);
    
    while (!list_is_empty(tag_a_eliminar->lista_bloques)) {
        list_remove_and_destroy_element(
            tag_a_eliminar->lista_bloques,
            0,
            liberar_bloque
        );
    }

    list_destroy(tag_a_eliminar->lista_bloques);

    config_destroy(tag_a_eliminar->metadata_config);
    pthread_mutex_destroy(&tag_a_eliminar->mutexListaBloquesTag);  
    pthread_mutex_destroy(&tag_a_eliminar->mutexMetadataTag);  

    free(tag_a_eliminar);
}

void liberar_bloque(void* ptr) {
    int* num_bloque = (int*) ptr;
    free(num_bloque);
}
