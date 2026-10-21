#include "operaciones_fs.h"

#include <errno.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/crypto.h>
#include <sys/stat.h>
#include <utils/utils.h>
#include <math.h>
#include <unistd.h>
#include "util.h"
#include "../main.h"
#include <fcntl.h>      
#include <unistd.h>    
#include <sys/mman.h>   
#include <sys/stat.h>   
#include <sys/types.h>

static int get_estado_bloque_bitmap(int num_bloque);
static int setear_bloque_libre_bitmap(int num_bloque);
static int primer_bloque_libre_bitmap();
static int crear_empty_metadata_config(char** path_out, char* file_name, char* tag_name);

static const unsigned char bitmap_ocupado = 1;
static const unsigned char bitmap_libre = 0;

/**
 *
 *  @returns -3 si el file existe
 */
int crear_estructura_file(t_file* file_out, char* nombre_file, int pid_create, char* nombre_tag)
{
    // Generar path del archivo
    char* path = malloc(sizeof(char) * PATH_MAX);
    if (get_path_by_file_name(path, nombre_file) < 0)
    {
        log_error(logger, "Error generando path, creacion de archivo");
        free(path);
        return -1;
    }
    // Crear estructura base del archivo
    const int error = crear_directorio(path);
    if (error < 0)
    {
        free(path);
        if (errno == -1)
        {
            return -3; // Error file exists
        }
        log_error(logger, "Error creando directorio, creacion de archivo");
        return -2;
    }
    file_out->dict_tags = dictionary_create();
    file_out->path = path;
    file_out->name = strdup(nombre_file);
    pthread_mutex_init(&file_out->mutexDictTags, NULL);
     //log obligatorio
     log_info(logger, "##<QUERY_ID %d> - File Creado <%s>:<%s>",pid_create,nombre_file, nombre_tag);
    return 0;
}

/**
 *  @brief Creates tag dir, logical blocks dir and empty metadata config
 *  @returns -3 si el file existe
 */
int crear_estructura_tag(t_tag* tag_out, char* nombre_file, char* nombre_tag, int pid)
{
    int error = 0;
    // Generar path del archivo
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (get_path_by_file_name_and_tag(path, nombre_file, nombre_tag) < 0)
    {
        log_error(logger, "Error generando path, creacion de tag");
        free(path);
        return -1;
    }
    // Crear estructura base del tag
    error = crear_directorio(path);

    if (error < 0)
    {
        free(path);
        if (errno == -1)
        {
            return -3; // Error tag exists
        }
        log_error(logger, "Error creando directorio del propio tag, en la creacion de tag");
        return -2;
    }
  

    resetear_cstring(path);// limpia el path para reutilizarlo

    //crea el directorio de bloques logicos del tag
    get_logical_blocks_path_by_file_name_and_tag_name(path, nombre_file, nombre_tag);
    
    error = crear_directorio(path);

    if (error < 0)
    {
        free(path);
        log_error(logger, "Error creando  de bloques logicos, en la creacion de tag");
        return -2;
    }

    //creo el metadta.config del tag
    char* metadata_config_path = NULL;
    error = crear_empty_metadata_config(&metadata_config_path, nombre_file, nombre_tag);

    if (error < 0)
    {
        free(path);
        log_error(logger, "Error creando metadata.config, creacion de tag");
        if (error == -2)
        {
            return -2;
        }
        return -1;
    }
    free(path);

    tag_out->lista_bloques = list_create();
    tag_out->nombre_tag = strdup(nombre_tag);
    tag_out->metadata_config = config_create(metadata_config_path);
    pthread_mutex_init(&tag_out->mutexMetadataTag, NULL);
    pthread_mutex_init(&tag_out->mutexListaBloquesTag, NULL);


    if (tag_out->metadata_config == NULL)
    {
        log_error(logger, "Error setting metadata config in tag");
        return -1;
    }

     //log obligatorio
     log_info(logger,"##<QUERY_ID %d> - Tag creado <%s>:<%s>",pid,nombre_file,nombre_tag);
    return 0;
}

/**
 *
 * @brief Primero se debe llamar a reservar bloques, esta funcion no cambia el estado del bitmap (esto se hace en reservar bloques)
 */
int add_block_to_tag(char* file_name, char* tag_name, const int num_bloque_fisico, int add_at_index, int pid)
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

    pthread_mutex_lock(&tag->mutexListaBloquesTag);
    int num_bloque_logico = list_size(tag->lista_bloques);
    pthread_mutex_unlock(&tag->mutexListaBloquesTag);

    aumentar_bloques_en_metadata_config(tag, num_bloque_fisico);
    
    pthread_mutex_lock(&mutexListaBloquesFisicos);
    t_bloque_fisico* bloque = list_get(lista_bloques_fisicos, num_bloque_fisico);
    pthread_mutex_unlock(&mutexListaBloquesFisicos);

    if (bloque == NULL)
    {
        log_error(logger, "Error obteniendo bloque de lista");
        return -5;
    }
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (get_logical_block_path(path, file_name, tag_name, num_bloque_logico) < 0)  
    {
        log_error(logger, "Error adding block to tag");
        free(path);
        return -1;
    }
    // Crear hardlink
    if (generar_hard_link(bloque->ruta_del_bloque_fisico, path) < 0)
    {
        log_error(logger, "Error generando hardlink");
        free(path);
        return -6;
    }
    
    //  sumar_inode_de_bloque(num_bloque_fisico);

    int* block_num_ptr = malloc(sizeof(int));
    *block_num_ptr = num_bloque_fisico;

    if (add_at_index >= 0)
    {
        pthread_mutex_lock(&tag->mutexListaBloquesTag);
        list_add_in_index(tag->lista_bloques, add_at_index, block_num_ptr);
        pthread_mutex_unlock(&tag->mutexListaBloquesTag);
        //log obligatorio
        log_info(logger, "##<QUERY_ID %d> - <%s>:<%s> Se agregó el hard link del bloque lógico <%d> al bloque físico <%d>",pid,file_name,tag_name,add_at_index,num_bloque_fisico); 

    } else
    {
        pthread_mutex_lock(&tag->mutexListaBloquesTag);
        list_add(tag->lista_bloques, block_num_ptr);
        int bloque_logico_num = list_size(tag->lista_bloques) - 1;
        pthread_mutex_unlock(&tag->mutexListaBloquesTag);
        //log obligatorio
        
        log_info(logger, "##<QUERY_ID %d> - <%s>:<%s> Se agregó el hard link del bloque lógico <%d> al bloque físico <%d>",pid,file_name,tag_name,bloque_logico_num,num_bloque_fisico);

    }
    free(path);
    return 0;
}

int add_block_to_tag_en_write(char* file_name, char* tag_name, int num_bloque_fisico,int num_bloque_logico, int pid){

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

    
    aumentar_bloques_en_metadata_config_en_write(tag, num_bloque_fisico, num_bloque_logico); 
    
    t_bloque_fisico* bloque = list_get(lista_bloques_fisicos, num_bloque_fisico);

    if (bloque == NULL)
    {
        log_error(logger, "Error obteniendo bloque de lista");
        return -5;
    }
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (get_logical_block_path(path, file_name, tag_name, num_bloque_logico) < 0)  
    {
        log_error(logger, "Error adding block to tag");
        free(path);
        return -1;
    }
    // Crear hardlink
    if (generar_hard_link(bloque->ruta_del_bloque_fisico, path) < 0)
    {
        log_error(logger, "Error generando hardlink");
        free(path);
        return -6;
    }
    
    // sumar_inode_de_bloque(num_bloque_fisico);

    int* block_num_ptr = malloc(sizeof(int));
    *block_num_ptr = num_bloque_fisico;

    list_add_in_index(tag->lista_bloques,num_bloque_logico ,block_num_ptr);
       
    //log obligatorio
 
    log_info(logger, "##<QUERY_ID %d> - <%s>:<%s> Se agregó el hard link del bloque lógico <%d> al bloque físico <%d>",pid,file_name,tag_name, num_bloque_logico,num_bloque_fisico);

    
    free(path);
    return 0;
}
int write_to_block(char* file_name, char* tag_name, int logical_block_number, int physical_block_number, char* data)
{

    char* path = malloc(sizeof(char) * PATH_MAX);

    if (get_logical_block_path(path, file_name, tag_name, logical_block_number) < 0)
    {
        log_error(logger, "Error generando path, escritura a bloque");
        free(path);
        return -1;
    }

    t_bloque_fisico* bloque = list_get(lista_bloques_fisicos, physical_block_number);

    pthread_mutex_lock(&bloque->mutexParaLeerOEscribir);
   
    int fd = open(bloque->ruta_del_bloque_fisico, O_RDWR);

    if (fd < 0) return -1;

    
    char* ptr = mmap(NULL, super_config->block_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        return -1;
    }

   
    memcpy(ptr, data, super_config->block_size);

    msync(ptr, super_config->block_size, MS_SYNC);

    munmap(ptr, super_config->block_size);
    close(fd);
    
    pthread_mutex_unlock(&bloque->mutexParaLeerOEscribir);

    return 0;
}

int crear_empty_metadata_config(char** path_out, char* file_name, char* tag_name)
{
    char* path = malloc(sizeof(char) * PATH_MAX);
    if (get_metadata_config_path_by_file_name_and_tag_name(path, file_name, tag_name) < 0)
    {
        log_error(logger, "Error creando el path del metadata config");
        free(path);
        return -1;
    }
    *path_out = path;

    FILE* archivo_metadata_config = fopen(path,"w+");

    if (archivo_metadata_config == NULL) {
        log_error(logger, "No se pudo crear el archivo metadata.config en el directorio %s", path);
        free(path);
        perror("fopen");
        return -2;
    }     
    fprintf(archivo_metadata_config, "TAMAÑO=0\n");
    fprintf(archivo_metadata_config, "BLOCKS=[]\n");
    fprintf(archivo_metadata_config, "ESTADO=%s\n", METADATA_CONFIG_DEFAULT_STATUS);
    fclose(archivo_metadata_config);
    return 0;
}
int reservar_bloques_por_primera_vez(int* bloques, int cant, int pid){
    int bloqueFisicoCero = 0;
    
    for(int i=0; i<cant; i++){
        bloques[i] = bloqueFisicoCero;
        usleep(configuration->retardo_acceso_bloque*1000); 
        //log obligatrio
        log_info(logger, "##<QUERY_ID %d> - Bloque Físico Reservado - Número de Bloque: <%d>",pid,bloqueFisicoCero);
    }
    return 0;
}

int reservar_bloques(int* bloques, int cant, int pid)
{
    for (int i = 0; i < cant; i++)
    {
        int bloque = primer_bloque_libre_bitmap(); //si me retorna -1 significa que todos lo bloques estan ocupados

        if (bloque == -1)
        {
            // Libero todos los bloques que ocupé en caso de que no alcancén a asignarme todos los bloques que necesito para hacer el truncate porque no hay más bloques disponibles
            for (int ir = i - 1; ir >= 0; ir--) 
            {
                if (setear_bloque_libre_bitmap(bloques[ir]) < 0)
                {
                    log_error(logger, "Error liberando bloques reservados");
                    exit(EXIT_FAILURE);
                }
            }
            log_error(logger, "No quedan bloques libres para reservar, y el tag no pudo reservar ni un bloque");
            return -2; // No quedan bloques disponibles -> CONTROLADO 
        }

        if (setear_bloque_ocupado_bitmap(bloque) < 0)
        {
            log_error(logger, "Error reservando bloques");
            // Libero todos los bloques que ocupé
            for (int ir = i; ir >= 0; ir--)
            {
                if (setear_bloque_libre_bitmap(bloque) < 0)
                {
                    log_error(logger, "Error liberando bloques reservados");
                    exit(EXIT_FAILURE);
                }
            }
            return -2; // Error causado por setear bloque ocupado (solo pasa si algo falla en el memcpy) -> CONTROLADO
        }
        bloques[i] = bloque;
        usleep(configuration->retardo_acceso_bloque*1000); 
        //log obligatrio
        log_info(logger, "##<QUERY_ID %d> - Bloque Físico Reservado - Número de Bloque: <%d>",pid,bloque);
    }
    return 0;
}

int primer_bloque_libre_bitmap(){
    pthread_mutex_lock(&mutexBitarrayBloques);
    for(int i=0;i<CANT_BLOQUES;i++){
        if(bitarray_test_bit(bitarray_bloques,i) == 0){
            pthread_mutex_unlock(&mutexBitarrayBloques);
            return i;
        }
    }
    pthread_mutex_unlock(&mutexBitarrayBloques);
    return -1;
}

int get_estado_bloque_bitmap(int num_bloque)
{
    return bitarray_test_bit(archivo_bitmap_bloques_mapeado, num_bloque);
}

int setear_bloque_ocupado_bitmap(int num_bloque) {
    pthread_mutex_lock(&mutexBitarrayBloques);
   // if (memcpy(archivo_bitmap_bloques_mapeado + num_bloque, &bitmap_ocupado,1) == NULL) //seteo como ocupado en el archivo mapeado del bitmap
  //  {
    //    pthread_mutex_unlock(&mutexBitarrayBloques);
  //      return -1;
   // }
    bitarray_set_bit(bitarray_bloques, num_bloque);
    pthread_mutex_unlock(&mutexBitarrayBloques);
    return 0;
}

int setear_bloque_libre_bitmap(int num_bloque) {
    pthread_mutex_lock(&mutexBitarrayBloques);
    
 //   if (memcpy(archivo_bitmap_bloques_mapeado + num_bloque, &bitmap_libre,1) == NULL) //seteo como libre en el archivo mapeado del bitmap
//{
  //      pthread_mutex_unlock(&mutexBitarrayBloques);
    //    return -1;
    //}
    
    bitarray_clean_bit(bitarray_bloques, num_bloque); //seteo como libre en el bitarray
    pthread_mutex_unlock(&mutexBitarrayBloques);
    return 0;
}

/**
 *
 * @brief Eliminar el bloque de la lista de bloques del tag luego de llamar a esta funcion
 */
int eliminar_bloque_logico(char* file_name, char* tag_name, int physical_block_num, int logical_block_num, int pid){

    char* path = malloc(sizeof(char) * PATH_MAX);

    if (get_logical_block_path(path, file_name, tag_name, logical_block_num) < 0)
    {
        log_error(logger, "Error generando path, eliminar bloque logico");
        free(path);
        return -1;
    }
    //lo desvinculo de su  con remove

    // char cwd[2048];
    // getcwd(cwd, sizeof(cwd));
    // printf("CWD = %s\n", cwd);


    int valor_remove = remove(path);
    // int valor_restar_inodo = restar_inode_de_bloque(physical_block_num); // ya no lo uso porque uso stat

    if ( valor_remove < 0)
    {
        if (errno == ENOENT)
        {
            log_warning(logger, "El archivo de bloque lógico del %s:%s que se quiere borar no existe",file_name,tag_name);
            return -2;
        } else
        {
            perror("Error deleting logical block");
            log_error(logger, "Error deleting logical block");
            if (setear_bloque_ocupado_bitmap(physical_block_num) < 0)
            {
                log_error(logger, "Error revirtiendo bloque en bitmap por error");
                exit(EXIT_FAILURE);
            }
            return -2;
        }
    }
     //log obligatorio
     log_info(logger, "##<QUERY_ID> <%d> - <%s>:<%s> Se eliminó el hard link del bloque lógico <%d> al bloque físico <%d>", pid, file_name,tag_name,logical_block_num,physical_block_num);
   
     // Liberar bloque del bitmap si no es referenciado por otro tag
    if (!cant_hardlinks_de_bloque(physical_block_num)){
        if (setear_bloque_libre_bitmap(physical_block_num) < 0)
        {
            log_error(logger, "Error seteando bloque no duplicado como libre, no deberia ocurrir");
            exit(EXIT_FAILURE); // Irrecuperable ya que esto nunca deberia poder ocurrir
        }

        //log obligatorio
        log_info(logger, "##<QUERY_ID> <%d> - Bloque Físico Liberado - Número de Bloque: <%d>",pid,physical_block_num);
       
    }
    return 0;
}

int update_tag_status(char* file_name, char* tag_name, char* status)
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
    pthread_mutex_lock(&tag->mutexMetadataTag);
    config_set_value(tag->metadata_config, METADATA_CONFIG_STATUS, status);
    pthread_mutex_unlock(&tag->mutexMetadataTag);

    if (config_save(tag->metadata_config) < 0)
    {
        log_error(logger, "Error saving config");
        return -3;
    }
    return 0;
}


void disminuir_size_en_metadata_config(t_tag* tag, int cantidad){
    pthread_mutex_lock(&tag->mutexMetadataTag);
    int tamaño_actual = config_get_int_value(tag->metadata_config, METADATA_CONFIG_SIZE);
    pthread_mutex_unlock(&tag->mutexMetadataTag);

    tamaño_actual = tamaño_actual - (super_config->block_size * cantidad);
    char* nuevo_tam = string_itoa(tamaño_actual);

    pthread_mutex_lock(&tag->mutexMetadataTag);
    config_set_value(tag->metadata_config, METADATA_CONFIG_SIZE, nuevo_tam);
    config_save(tag->metadata_config);
    pthread_mutex_unlock(&tag->mutexMetadataTag);
    free(nuevo_tam);
}

void aumentar_size_en_metadata_config(t_tag* tag, int cant_bloques_a_reservar){
    //aumento el tamaño , y lo pongo en la metadta.config
    pthread_mutex_lock(&tag->mutexMetadataTag);
    int tamaño_actual = config_get_int_value(tag->metadata_config, METADATA_CONFIG_SIZE);
    pthread_mutex_unlock(&tag->mutexMetadataTag);

    tamaño_actual = tamaño_actual + (cant_bloques_a_reservar * super_config->block_size);
    char* nuevo_tam = string_itoa(tamaño_actual);

    pthread_mutex_lock(&tag->mutexMetadataTag);
    config_set_value(tag->metadata_config, METADATA_CONFIG_SIZE, nuevo_tam);
    config_save(tag->metadata_config);
    pthread_mutex_unlock(&tag->mutexMetadataTag);
    free(nuevo_tam);
}

void aumentar_bloques_en_metadata_config(t_tag* tag, int num_bloque_fisico){

     //agrego el bloque fisico al array de bloques en metadata.config
     pthread_mutex_lock(&tag->mutexMetadataTag);

     char** blocks = config_get_array_value(tag->metadata_config, METADATA_CONFIG_BLOCKS);
     if (blocks == NULL)
     {
         log_error(logger, "Error adding block to tag, error creating config");
         pthread_mutex_unlock(&tag->mutexMetadataTag);
         return ;
     }
     char* block_str = string_itoa(num_bloque_fisico);
     string_array_push(&blocks, block_str);
     char* blocks_as_cstring = join_strings(blocks, string_array_size(blocks));
     config_set_value(tag->metadata_config, METADATA_CONFIG_BLOCKS, blocks_as_cstring);

     pthread_mutex_unlock(&tag->mutexMetadataTag);

     if (config_save(tag->metadata_config) < 0)
     {
        log_error(logger, "Error adding block to tag, error saving config");
        free(block_str);
        // string_array_destroy(blocks);
        free(blocks_as_cstring);
        return;
     }

     free(block_str);
    //  string_array_destroy(blocks);
     free(blocks_as_cstring);
}

void disminuir_bloques_en_metadata_config(t_tag* tag, int num_bloque_fisico, int num_bloque_logico){

     //quito el bloque fisico al array de bloques en metadata.config
     pthread_mutex_lock(&tag->mutexMetadataTag);

     char** blocks = config_get_array_value(tag->metadata_config, METADATA_CONFIG_BLOCKS);
    
     if (blocks == NULL)
     {
         log_error(logger, "Error adding block to tag, error creating config");
         pthread_mutex_unlock(&tag->mutexMetadataTag);
         return;
     }

    char* block_str = string_itoa(num_bloque_fisico);
    char** nuevo_array = quitar_bloque_del_array(blocks, block_str,num_bloque_logico);
    char* nuevo_array_como_string = join_strings(nuevo_array, string_array_size(nuevo_array));
    config_set_value(tag->metadata_config, METADATA_CONFIG_BLOCKS, nuevo_array_como_string);

    pthread_mutex_unlock(&tag->mutexMetadataTag);

     if (config_save(tag->metadata_config) < 0)
     {
         log_error(logger, "Error adding block to tag, error saving config");
         free(block_str);
        //  string_array_destroy(blocks);
         string_array_destroy(nuevo_array);
         return;
     }

    free(block_str);
    // string_array_destroy(blocks);
    string_array_destroy(nuevo_array);
}


char** quitar_bloque_del_array(char** array_anterior, char* num_bloque, int num_bloque_logico){

    int tam = string_array_size(array_anterior);
    int nuevo_tam = tam -1;


    char** nuevo_array = calloc(nuevo_tam + 1, sizeof(char*)); 
    int pos = 0;

    for (int i = 0; i < tam; i++) {
        if ( i != num_bloque_logico){
            nuevo_array[pos++] = strdup(array_anterior[i]);
        }
    }

    nuevo_array[pos] = NULL;

    return nuevo_array;
}

void aumentar_bloques_en_metadata_config_en_write(t_tag* tag, int num_bloque_fisico, int num_bloque_logico){

    pthread_mutex_lock(&tag->mutexMetadataTag);

    char** blocks = config_get_array_value(tag->metadata_config, METADATA_CONFIG_BLOCKS);
    if (!blocks) {
        log_error(logger, "Error: metadata_config blocks is NULL");
        pthread_mutex_unlock(&tag->mutexMetadataTag);
        return;
    }
    int tam = string_array_size(blocks);

    if (num_bloque_logico >= tam) {
        log_error(logger, "El bloque lógico está fuera de rango");
        string_array_destroy(blocks);
        pthread_mutex_unlock(&tag->mutexMetadataTag);
        return;
    }
    // Reemplazo
    free(blocks[num_bloque_logico]);  // liberar la string vieja
    blocks[num_bloque_logico] = string_itoa(num_bloque_fisico);
    // Convertir a string para el config
    char* blocks_as_cstring = join_strings(blocks, tam);
    config_set_value(tag->metadata_config, METADATA_CONFIG_BLOCKS, blocks_as_cstring);

    pthread_mutex_unlock(&tag->mutexMetadataTag);

    if (config_save(tag->metadata_config) < 0) {
        log_error(logger, "Error guardando metadata.config");
    }

    free(blocks_as_cstring);
    string_array_destroy(blocks);
}
