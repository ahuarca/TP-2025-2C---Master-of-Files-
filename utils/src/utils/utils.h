#ifndef UTILS_H
#define UTILS_H
#include <commons/log.h>
#include <commons/collections/dictionary.h>
#include <math.h>

t_log* init_logger(char* fileName, char* log_name);

void destroy_logger(t_log* logger);

int extract_args(t_dictionary* out_args, int argc, char* argv[]);

int build_path(
    char* path, size_t path_sz, const char* first, ...
);

void resetear_cstring(char* cstring);

char* create_zero_string(size_t size);

int open_for_truncate_if_free(const char *path, FILE **out);


#endif
