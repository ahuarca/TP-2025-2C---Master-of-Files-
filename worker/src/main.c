#include "./include/worker.h"

int main(int argc, char* argv[]) {

    if(argc<3){
        perror("No se indicaron los parametros suficientes. Finalizando ejecucion...");
        return 1;
    }

    cargar_argumentos(argv);
    iniciar_worker();
    iniciar_query_interpreter();
    return 0;
}