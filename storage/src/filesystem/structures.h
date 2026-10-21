#ifndef STRUCTURES_H
#define STRUCTURES_H
#include <commons/config.h>
#include <pthread.h>
#define NOMBRE_FILE_DIR "files"
#define LOGICAL_BLOCKS_DIRNAME "logical_blocks"
#define METADATA_CONFIG_NAME "metadata.config"
#define METADATA_CONFIG_DEFAULT_STATUS "WORK_IN_PROGRESS"
#define METADATA_CONFIG_STATUS_COMMITED "COMMITED"
#define METADATA_CONFIG_SIZE "TAMAÑO"
#define METADATA_CONFIG_BLOCKS "BLOCKS"
#define METADATA_CONFIG_STATUS "ESTADO"

typedef struct {
    char* path_completo;       // /home/.../initial_file/BASE
    char* nombre_directorio;   // initial_file
    char* nombre_tag;          // BASE
} file_tag_info_t;

typedef struct {
    file_tag_info_t* array_paths;
    int cantidad;
} dir_lista_t;

typedef struct 
{
    int block_size;
    int fs_size;
    
}t_config_super_block;


// Del worker
typedef struct{
    char* operacion;
    char* nombre_file;
    char* tag;
    int primer_parametro_entero;
    int segundo_parametro_entero;
    char* primer_parametro_cadena;
    char* segundo_parametro_cadena;
}t_worker_instruccion;

typedef struct{
    int inode_cant;
    char* nombre_bloque_fisico;
    char* ruta_del_bloque_fisico;
    void* mmap_ptr;
    pthread_mutex_t mutex_BF;// para restar o sumar el inodos
    pthread_mutex_t mutexParaLeerOEscribir;// para leer o escribir el archivo del bloque
}t_bloque_fisico;

typedef struct
{
    char* nombre_tag;
    t_list* lista_bloques;
    t_config* metadata_config;
    pthread_mutex_t mutexMetadataTag;
    pthread_mutex_t mutexListaBloquesTag;
}t_tag;

typedef struct
{
    t_dictionary* dict_tags;
    char* path;
    char* name;
    pthread_mutex_t mutexDictTags;
}t_file;

#endif
