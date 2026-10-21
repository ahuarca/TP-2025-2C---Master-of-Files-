#include "include/main_master.h"

int main(int argc, char* argv[]) {
    signal(SIGINT, manejar_ctrl_c); 
    //INICIALIZACION 
    if(argc<2){
        perror("No se indicaron los parametros suficientes. Finalizando ejecucion...");
        return 1;
    }
    archivo_config= argv[1];
    
    inicializacion();

    //ESPERAR CONEXIONES 
    pthread_t escucha_de_clientes;
    pthread_create(&escucha_de_clientes, NULL, conexiones_con_clientes, NULL); 

    //PLANIFICADOR 
    pthread_t planificador_corto_plazo;
    pthread_create(&planificador_corto_plazo ,NULL,planificador_corto,NULL);

    //FINALIZADOR (capaz se innecesario pordria ser una funcion y ya esta, en vez de un hilo)
    pthread_t finalizador_de_queries;
    pthread_create(&finalizador_de_queries,NULL,finalizador ,NULL);

    pthread_join(planificador_corto_plazo,NULL);
    pthread_join(escucha_de_clientes,NULL);
    pthread_join(finalizador_de_queries,NULL);

    return 0;
}
