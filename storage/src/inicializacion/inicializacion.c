#include "inicializacion.h"
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
#include <commons/config.h>
#include <utils/utils.h>
#include "../main.h"
#include "../filesystem/util.h"
#include "../filesystem/operaciones_fs.h"
#include <stdio.h>
#include <stdlib.h>


static int crear_super_config();
static int crear_directorios();
static int crear_archivos();
static int crear_bloques_fisicos();
static int crear_bitmap_bloques();
static int crear_initial_file();
static int crear_bitarray();
static int crear_config_hash();
static int eliminar_archivos_storage();
int eliminar_fs();


int fd_bitmap;

int inicializar_storage()
{
    if (
        eliminar_fs() < 0
        // || eliminar_archivos_storage() < 0
        || crear_directorios() < 0
        || crear_archivos() < 0
        || crear_bloques_fisicos() < 0
        || crear_initial_file() < 0
    )
    {
        log_error(logger, "Error inicializacion");
        return -1;
    }
    return 0;
}

int eliminar_fs()
{
    if (directorio_existe(configuration->punto_montaje))
    {
        if (eliminar_archivos_storage() < 0)
        {
            log_error(logger, "Error eliminando archivos");
            return -1;
        }
        char* path = malloc(sizeof(char) * PATH_MAX);

        if (build_path(path, PATH_MAX,configuration->punto_montaje, "physical_blocks", NULL) < 0)
        {
            perror("eliminar_fs build_path failed");
            free(path);
            return -1;
        }
        if (eliminar_archivos_de_directorio(path) < 0)
        {
            log_error(logger, "Error eliminando physical blocks");
            free(path);
            return -2;
        }
        path[0] = '\0';

        if (build_path(path, PATH_MAX,configuration->punto_montaje, "files", NULL) < 0)
        {
            perror("eliminar_fs build_path failed");
            free(path);
            return -1;
        }
        if (eliminar_archivos_de_directorio(path) < 0)
        {
            log_error(logger, "Error eliminando files");
            free(path);
            return -2;
        }
        free(path);
    }
    return 0;
}

int eliminar_superblock_config()
{
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (build_path(
        path, PATH_MAX,
        configuration->punto_montaje, "superblock.config", NULL
    ) < 0)
    {
        perror("eliminar_superblock_config build_path failed");
        free(path);
        return -1;
    }
    if (remove(path) < 0)
    {
        log_error(logger, "No se pudo eliminar el archivo superblock.config");
        free(path);
        return -2;
    }
    log_debug(logger, "Se elimino el archivo superblock.config");
    free(path);
    return 0;
}

int eliminar_archivos_storage()
{
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (build_path(path, PATH_MAX,configuration->punto_montaje, "bitmap.bin", NULL) < 0){
        perror("eliminar_archivos_nativos_storage build_path failed");
        free(path);
        return -1;
    }
        if (remove(path) == 0)
        {
            log_debug(logger, "Se elimino el archivo bitmap.bin");
        }
        else
        {
            if (errno != ENOENT)
            {
                log_error(logger, "No se pudo eliminar el archivo bitmap.bin");
                free(path);
                return -2;
            }
        }

    path[0] = '\0';

    if (build_path( path, PATH_MAX,configuration->punto_montaje, "blocks_hash_index.config", NULL) < 0){
        perror("eliminar_archivos_nativos_storage build_path failed");
        free(path);
        return -1;
    }
        if (remove(path) == 0)
        {
            log_debug(logger, "Se elimino el archivo blocks_hash_index.config");
        }
        else
        {
            if (errno != ENOENT)
            {
                log_error(logger, "No se pudo eliminar el archivo blocks_hash_index.config");
                free(path);
                return -2;
            }
        }     

    free(path);

    eliminar_superblock_config();


    return 0;
}

int crear_initial_file()
{
    char* initial_file = "initial_file";
    char* base_tag = "BASE";
    t_file* file = malloc(sizeof(t_file));
    t_tag* tag = malloc(sizeof(t_tag));
    int error = crear_estructura_file(file, initial_file, -1, base_tag);

    if (error != 0)
    {
        log_error(logger, "Error inicializacion");
        return -1;
    }
    error = crear_estructura_tag(tag, initial_file, base_tag, -1);

    if (error < 0)
    {
        log_error(logger, "Error inicializacion");
        return -6;
    }
    dictionary_put(file->dict_tags, tag->nombre_tag, tag);
    dictionary_put(dict_files, file->name, file);

    if (error != 0)
    {
        log_error(logger, "Error inicializacion");
        return -2;
    }
    // Setear bloque ocupado
    if (setear_bloque_ocupado_bitmap(0) < 0)
    {
        log_error(logger, "Error reservando bloque");
        return -9;
    }
    error = add_block_to_tag(initial_file, base_tag, 0, -1, -1);

    if (error != 0)
    {
        log_error(logger, "Error inicializacion");
        return -3;
    }

  
    // char* zeros = create_zero_string(super_config->block_size);

    // if (zeros == NULL)
    // {
    //     log_error(logger, "Error inicializacion, creando zero string");
    //     return -4;
    // }
    // error = write_to_block(initial_file, base_tag, 0, 0, zeros);

    // if (error != 0)
    // {
    //     log_error(logger, "Error inicializacion");
    //     return -5;
    // }

    return 0;
}

int crear_bitmap_bloques()
{
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (build_path(
        path, PATH_MAX,
        configuration->punto_montaje, "bitmap.bin", NULL
    ) < 0)
    {
        perror("crear_bitmap_bloques build_path failed");
        return -1;
    }
    fd_bitmap = open(path, O_RDWR | O_CREAT, 0666);

    if (fd_bitmap < 0)
    {
        perror("open");
        log_error(logger, "Error abriendo archivo bitmap en %s", path);
        return -2;
    }

    off_t tam = CANT_BLOQUES/8;

    if (ftruncate(fd_bitmap, tam) < 0)
    {
        perror("ftruncate");
        close(fd_bitmap);
        log_error(logger, "No se pudo truncar el bitmap");
        return -3;
    }
    archivo_bitmap_bloques_mapeado = mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitmap, 0);
    
    // cada byte del archivo se representara con dos caracteres hex

    if (archivo_bitmap_bloques_mapeado == MAP_FAILED)
    {
        perror("mmap");
        close(fd_bitmap);
        log_error(logger, "No se pudo mapear el bitmap");
        return -4;
    }
    //creo el bitarray de verdad
    crear_bitarray();
    close(fd_bitmap);
    return 0;
}

int crear_bitarray()
{
    const size_t n_bloques = (size_t)CANT_BLOQUES / 8;
    // aca son 4 bytes = 32 bits, porque el bitarray se manipula en bits-> cada bit representa un bloque
    // char* buffer = malloc(n_bloques);
    memset(archivo_bitmap_bloques_mapeado, 0, n_bloques);
    // memset(buffer, 0, n_bloques);
    bitarray_bloques = bitarray_create_with_mode(archivo_bitmap_bloques_mapeado, n_bloques , LSB_FIRST);

    if (bitarray_bloques == NULL)
    {
        log_error(logger, "No se creo el puntero del bitarray");
        return -1;
    }
    return 0;
}

int crear_archivos()
{
    if (crear_super_config() < 0)
    {
        log_error(logger, "Error creando super config");
        return -1;
    }
    log_debug(logger, "¡Se creo exitosamente el archivo superBlock!");

    if (crear_bitmap_bloques() < 0)
    {
        log_error(logger, "Error creando bitmap");
        return -2;
    }
    log_debug(logger, "¡Se creo exitosamente el archivo bitmap!");
    if (crear_config_hash() < 0)
    {
        log_error(logger, "Error creando bitmap");
        return -3;
    }
    log_debug(logger, "¡Se creo exitosamente el archivo blocks_hash_index.config!");
    return 0;
}

int crear_directorios()
{
    if (mkdir(configuration->punto_montaje, 0777) == -1)
    {
        if (errno == EEXIST)
        {
            log_debug(logger, "El directorio /home/utnso/storage ya existe");
        }
        else
        {
            log_error(logger, "Ocurrió un error al crear el directorio %s", configuration->punto_montaje);
            return -1;
        }
    }
    else
    {
        log_debug(logger, "Se creo exitosamente el directorio %s!", configuration->punto_montaje);
    }
    char* path = malloc(sizeof(char) * PATH_MAX);

    if (build_path(path, PATH_MAX, configuration->punto_montaje, "physical_blocks", NULL) < 0)
    {
        perror("crear_directorios_nativos_storage build_path failed");
        free(path);
        return -2;
    }
    if (mkdir(path, 0777) == -1)
    {
        if (errno == EEXIST)
        {
            log_debug(logger, "El directorio %s ya existe", path);
            
        }
        else
        {
            log_error(logger, "Ocurrió un error al crear el directorio %s", path);
            free(path);
            return -1;
        }
    }
    else
    {
        log_debug(logger, "Se creo exitosamente el directorio %s!", path);
    }

    path[0] = '\0';
    if (build_path(path, PATH_MAX, configuration->punto_montaje, "files", NULL) < 0)
    {
        perror("crear_directorios_nativos_storage build_path failed");
        free(path);
        return -2;
    }
    if (mkdir(path, 0777) == -1)
    {
        if (errno == EEXIST)
        {
            log_debug(logger, "El directorio %s ya existe", path);
        }
        else
        {
            log_error(logger, "Ocurrió un error al crear el directorio  %s", path);
            free(path);
            return -1;
        }
    }
    else
    {
        log_debug(logger, "Se creo exitosamente el directorio  %s!", path);
    }
    free(path);
    return 0;
}

int crear_super_config()
{
    char* path = malloc(sizeof(char) * PATH_MAX);
    if (build_path(
        path, PATH_MAX,
        configuration->punto_montaje, "superblock.config", NULL
    ) < 0)
    {
        perror("crear_super_config build_path failed");
    }
    t_config* super_block_conf = config_create(path);
    super_config = malloc(sizeof(t_config_super_block));

    if (super_block_conf == NULL)
    {
        // Create it if it does not exist
        FILE* file = fopen(path, "w");

        if (file == NULL)
        {
            perror("Error creando archivo superblock");
            log_error(logger, "Error creando superblock.config at %s", path);
            return -1;
        }
        fprintf(file, "BLOCK_SIZE=%d\n", configuration->initial_block_size);
        fprintf(file, "FS_SIZE=%d\n", configuration->initial_fs_size);
        fclose(file);
        super_config->block_size = configuration->initial_block_size;
        super_config->fs_size = configuration->initial_fs_size;
        super_block_conf = config_create(path);
    }
    else
    {
        // Read it
        if (!config_has_property(super_block_conf, "BLOCK_SIZE"))
        {
            printf("Error reading BLOCK_SIZE from super block config");
            config_destroy(super_block_conf);
            return -2;
        }
        super_config->block_size = config_get_int_value(super_block_conf, "BLOCK_SIZE");
        if (!config_has_property(super_block_conf, "FS_SIZE"))
        {
            printf("Error reading FS_SIZE from super block config");
            config_destroy(super_block_conf);
            return -2;
        }
        super_config->fs_size = config_get_int_value(super_block_conf, "FS_SIZE");
    }
    // config_destroy(super_block_conf);
    const int rest = super_config->fs_size % super_config->block_size;
    if (rest != 0)
    {
        printf("Super block config has invalid values, CANT_BLOQUES rest is %d", rest);
        return -3;
    }
    CANT_BLOQUES = super_config->fs_size / super_config->block_size;

    log_info(logger, "CANT BLOQUES = %d ", CANT_BLOQUES);

    return 0;
}

int crear_bloques_fisicos()
{
    char* path = malloc(sizeof(char) * PATH_MAX);
   
    for (int i = 0; i < CANT_BLOQUES; i++)
    {
        char filename[24];
        char* nombreBloqueFisicoFormateado = block_num_to_block_name(i);
        snprintf(filename, 24, "%s%s",nombreBloqueFisicoFormateado , ".dat");

        if (build_path(
            path, PATH_MAX,
            configuration->punto_montaje, "physical_blocks", filename, NULL
        ) < 0)
        {
            perror("crear_bloques_fisicos build_path failed");
            free(path);
            return -1;
        }
        const int fd_bloque = open(path, O_RDWR | O_CREAT, 0666);

        if (fd_bloque == -1)
        {
            perror("open");
            return -2;
        }
        if (ftruncate(fd_bloque, super_config->block_size) < 0)
        {
            perror("ftruncate");
            close(fd_bloque);
            free(path);
            log_error(logger, "No se pudo truncar el archivo del bloque");
            return -3;
        }
        void* mapeo_bloque = mmap(NULL, super_config->block_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloque, 0);
        close(fd_bloque);

        if (mapeo_bloque == MAP_FAILED)
        {
            perror("mmap");
            free(path);
            log_error(logger, "No se pudo mapear el archivo del bloque");
            return -4;
        }

        if (i==0)
        {
            memset(mapeo_bloque, '0', super_config->block_size); //inicializo el bloque con cero si es bf 0
        }else{
            memset(mapeo_bloque, 0, super_config->block_size); //inicializo el bloque con /0
        }
        
       
        msync(mapeo_bloque, super_config->block_size, MS_SYNC);
        // munmap(mapeo_bloque, super_config->block_size);
        t_bloque_fisico* bloque_fisico = malloc(sizeof(t_bloque_fisico));

        if (bloque_fisico == NULL) {
            perror("fallo el malloc al crear el bloque_fisico");
            return -1; 
        }
        bloque_fisico->mmap_ptr = mapeo_bloque;
        bloque_fisico->inode_cant = 0; // Esto es el numero de tags usando al bloque, comienza en 0
        bloque_fisico->nombre_bloque_fisico = string_duplicate(nombreBloqueFisicoFormateado);
        free(nombreBloqueFisicoFormateado);
        bloque_fisico->ruta_del_bloque_fisico = string_duplicate(path);
        pthread_mutex_init(&bloque_fisico->mutex_BF, NULL);
        pthread_mutex_init(&bloque_fisico->mutexParaLeerOEscribir, NULL);
        // log_debug(logger, "Ruta del bloque fisico en la estructura %s", bloque_fisico->ruta_del_bloque_fisico);
        list_add(lista_bloques_fisicos, bloque_fisico);
    }
    free(path);
    
    return 0;
}

int crear_struct_para_t_bloque_fisico(int num_bloque_fisico){

    char* path = malloc(sizeof(char) * PATH_MAX);

    char filename[24];
    char* nombreBloqueFisicoFormateado = block_num_to_block_name(num_bloque_fisico);
    snprintf(filename, 24, "%s%s",nombreBloqueFisicoFormateado , ".dat");

        if (build_path(path, PATH_MAX,configuration->punto_montaje, "physical_blocks", filename, NULL) < 0){
            perror("crear_bloques_fisicos build_path failed");
            free(path);
            return -1;
        }

    t_bloque_fisico* bloque_fisico = malloc(sizeof(t_bloque_fisico));

        if (bloque_fisico == NULL) {
            perror("fallo el malloc al crear el bloque_fisico");
            return -1; 
        }

    bloque_fisico->mmap_ptr = NULL;
    bloque_fisico->inode_cant = 0; // Esto es el numero de tags usando al bloque, comienza en 0 -> ya no lo uso, ahora uso stat
    bloque_fisico->nombre_bloque_fisico = string_duplicate(nombreBloqueFisicoFormateado);
     free(nombreBloqueFisicoFormateado);
    bloque_fisico->ruta_del_bloque_fisico = string_duplicate(path);
    pthread_mutex_init(&bloque_fisico->mutex_BF, NULL);
    pthread_mutex_init(&bloque_fisico->mutexParaLeerOEscribir, NULL);
    // log_debug(logger, "Ruta del bloque fisico en la estructura %s", bloque_fisico->ruta_del_bloque_fisico);
    list_add(lista_bloques_fisicos, bloque_fisico);
    free(path);

    return 0;
}

int crear_config_hash()
{
    char* path = malloc(sizeof(char) * PATH_MAX);
    if (build_path(path, PATH_MAX, configuration->punto_montaje, "blocks_hash_index.config", NULL) < 0)
    {
        perror("crear_config_hash build_path failed");
        return -1;
    }
    archivo_block_hash_index = fopen(path, "w");

    if (archivo_block_hash_index == NULL)
    {
        perror("fopen");
        log_error(logger, "Error abriendo archivo block_hash_index en %s", path);
        return -2;
    }
    if (fclose(archivo_block_hash_index) < 0)
    {
        perror("fclose");
        log_error(logger, "Error cerrando archivo block_hash_index en %s", path);
        return -2;
    }
    //ahora si lo transformo en t_config
    config_hash = config_create(path);

    if (config_hash == NULL)
    {
        log_error(logger, "Error al crear la config del index hash");
        return -1;
    }

    return 0;
}

int crear_bitarray_para_fresh_false(){

    char* path_bitmap = malloc(sizeof(char) * PATH_MAX);
    
    int rta = build_path(path_bitmap,PATH_MAX,configuration->punto_montaje,"bitmap.bin",NULL);

    if(rta<0){
        log_error(logger, "ocurrio un error al crear el path del bitmap");
        free(path_bitmap);
        return -1;
    }

  //  uint8_t buffer[CANT_BLOQUES];

 //   FILE* f = fopen(path_bitmap, "rb");

 //       if (!f) {
  //          perror("fopen");
   //         exit(1);
   //     }
//
 //   if (fread(buffer, 1, CANT_BLOQUES, f) != CANT_BLOQUES) {
 //       perror("fread");
  //      fclose(f);
   //     exit(1);
  //  }

 //   fclose(f);

    //esto es para el archivo bitmap.bin
    int fd = open(path_bitmap, O_RDWR);
    
        if (fd == -1) { 
            perror("open"); 
            free(path_bitmap);
            exit(1); 
        }

   int tam = CANT_BLOQUES/8;
    
    archivo_bitmap_bloques_mapeado = mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        if (archivo_bitmap_bloques_mapeado == MAP_FAILED) {
            perror("mmap"); exit(1);
        }

    close(fd);  


    free(path_bitmap);
    
  //  size_t cant_bits = (size_t)CANT_BLOQUES / 8;

  //  char* buffer_para_bitarray = malloc(cant_bits);

    bitarray_bloques = bitarray_create_with_mode(archivo_bitmap_bloques_mapeado,tam,LSB_FIRST);

        if(bitarray_bloques==NULL){
            log_error(logger, "Hubo un eror al crear el bitarray el fresh start false");
        }



 //   for (int i = 0; i < CANT_BLOQUES; i++) {

      //  if (buffer[i] == 1){
         //   bitarray_set_bit(bitarray_bloques, i);
            // log_debug(logger, "Se volvió a  marcar como ocupado el bloque %d",i);
     //   }
        //no hace falta porque ya lo incializo en 0 todo
        // else{
        //     bitarray_clean_bit(bitarray_bloques, i);
        //     log_debug(logger, "Se volvió a marcar como libre el bloque %d",i);
        // }         
  //  }
    
    log_info(logger, "Bitarray cargado correctamente desde archivo");

    return 0;

}



int crear_config_hash_para_fresh_false(){

    char* path = malloc(sizeof(char) * PATH_MAX);
    if (build_path(path, PATH_MAX, configuration->punto_montaje, "blocks_hash_index.config", NULL) < 0)
    {
        log_error(logger, "falla al crear el path del blocks_hash_index.config ");
        perror("crear_config_hash build_path failed");
        return -1;
    }

     //ahora si lo transformo en t_config
     config_hash = config_create(path);

     if (config_hash == NULL)
     {
         log_error(logger, "Error al crear la config del index hash");
         return -1;
     }
     return 0;
}

