#include "utils.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <stdio.h>

#define LOG_DIR "logs"

static int append_path(char *path, size_t path_sz, const char *next);

t_log* init_logger(char* fileName, char* log_name)
{
    if (!fileName || !log_name)
    {
        printf("Invalid arguments to init log\n");
        return NULL;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), LOG_DIR "/%s", fileName);
    if (access(LOG_DIR, F_OK) != 0)
    {
        if (mkdir(LOG_DIR, 0777) != 0)
        {
            perror("Error al crear el directorio de logs\n");
        }
    }
    return log_create(path, log_name, true, LOG_LEVEL_TRACE);
}

void destroy_logger(t_log* logger)
{
    log_destroy(logger);
}

inline char* create_zero_string(const size_t size)
{
    
    char* buffer = malloc(size);
    memset(buffer, '0', size);
    buffer[size-1] = '\0';
    return buffer;
    
}

int extract_args(t_dictionary* out_args, const int argc, char* argv[])
{
    for (int i = 1; i + 1 < argc; i += 2)
    {
        char* key = argv[i];

        if (strlen(key) < 2 || (key[0] != '-' && key[1] != '-'))
        {
            printf("Malformed arg key: %s", key);
            return -1;
        }
        char* value = argv[i + 1];
        dictionary_put(out_args, key + 2, value);
    }
    return 0;
}

int build_path(char *path, size_t path_sz, const char *first, ...)
{
    if (!path || !first) { errno = EINVAL; return -1; }

    va_list args;
    va_start(args, first);

    path[0] = '\0';

    // iterate over all parts, including `first`, until NULL sentinel
    for (const char *part = first; part != NULL; part = va_arg(args, const char *)) {
        int n;
        if (path[0] == '\0') {
            n = snprintf(path, path_sz, "%s", part);
            if (n < 0 || (size_t)n >= path_sz) { va_end(args); errno = ENAMETOOLONG; return -1; }
        } else {
            n = append_path(path, path_sz, part);
            if (n < 0) { va_end(args); errno = ENAMETOOLONG; return -1; }
        }
    }

    va_end(args);
    return 0;
}
int append_path(char *path, size_t path_sz, const char *next) {
    if (!path || !next) { errno = EINVAL; return -1; }

    size_t len = strlen(path);
    const char *sep = (len > 0 && path[len - 1] == '/') || next[0] == '/'  ? "" : "/";

    int n = snprintf(path + len, path_sz - len, "%s%s", sep, next);

    if (n < 0 || (size_t)n >= path_sz - len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

void resetear_cstring(char* cstring)
{
    cstring[0] = '\0';
}

int open_for_truncate_if_free(const char *path, FILE **out) {
    *out = NULL;

    // 1) Abrir sin crear y sin truncar
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        if (errno == ENOENT) return -2; // no existe
        return -3; // otros errores
    }

    // 2) Lock exclusivo NO bloqueante
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        int e = errno;
        close(fd);
        if (e == EWOULDBLOCK) return -4; // en uso
        return -5; // otro error
    }

    // 3) Convertir a FILE* sin truncar
    FILE *f = fdopen(fd, "r+");  // <<< NOTA: r+ NO trunca
    if (!f) {
        int e = errno;
        flock(fd, LOCK_UN);
        close(fd);
        errno = e;
        return -7;
    }

    *out = f;
    return 0;
}

// int open_for_truncate_if_free(const char *path, FILE **out) {
//     *out = NULL;

//     // 1) Must exist: open write-only, NO O_CREAT, NO O_TRUNC yet.
//     int fd = open(path, O_WRONLY);
//     if (fd == -1) {
//         if (errno == ENOENT) {
//             // file does not exist
//             return -2;
//         }
//         // other open error (permissions, dirs, etc.)
//         return -3;
//     }

//     // 2) Try to take an exclusive advisory lock, non-blocking
//     if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
//         int e = errno;
//         close(fd);
//         if (e == EWOULDBLOCK) {
//             // another process holds a lock
//             return -4;
//         }
//         // some other locking error
//         return -5;
//     }

//     // 3) Now it’s safe to truncate to empty
//     // if (ftruncate(fd, 0) == -1) {
//     //     int e = errno;
//     //     flock(fd, LOCK_UN);
//     //     close(fd);
//     //     errno = e;
//     //     return -6;
//     // }

//     FILE *f = fdopen(fd, "w");
//     if (!f) {
//         int e = errno;
//         flock(fd, LOCK_UN);
//         close(fd);
//         errno = e;
//         return -7;
//     }

//     *out = f;
//     return 0; // success
// }
