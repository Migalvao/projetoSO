//PARA COMPILAR: gcc -Wall -pthread main.c -o main -lrt
#include "header.h"
 
int fd_pipe;    //PIPE
int shmid;      //SHARED MEMORY

estatisticas_sistema * estatisticas;

char comando[MAX_SIZE_COMANDO];

void torre_controlo(){
    //inicializar o array de partidas
    sem_wait(sem_partidas);
    for(int i=0; i < gs_configuracoes.qnt_max_partidas; i++)
        array_voos_partida[i].init = -1;
    sem_post(sem_partidas);

    //inicializar o array de partidas
    sem_wait(sem_chegadas);
    for(int i=0; i < gs_configuracoes.qnt_max_chegadas; i++)
        array_voos_chegada[i].init = -1;
    sem_post(sem_chegadas);

    sem_wait(sem_log);
    sprintf(mensagem, "Torre de controlo iniciada. Pid: %d", getpid());
    write_log(mensagem);
    sem_post(sem_log);

}

void gestor_simulacao(){
    char * command;
    pthread_t thread_criadora_partidas, thread_criadora_chegadas;
    time(&t_inicial);   //definir o tempo inicial, declarado em header.h
    pthread_create(&thread_criadora_partidas, NULL, criar_partida, NULL);
    pthread_create(&thread_criadora_chegadas, NULL, criar_chegada, NULL);
    sem_wait(sem_log);
    sprintf(mensagem, "Gestor de simulação iniciado. Pid: %d",getpid());
    write_log(mensagem);
    sem_post(sem_log);

    while(1){
        read(fd_pipe,comando,MAX_SIZE_COMANDO);
        command = strtok(comando, "\n");
        if(strcmp(command, "exit") == 0){
            sem_wait(sem_log);
            sprintf(mensagem, "Servidor terminado");
            write_log(mensagem);
            sem_post(sem_log);
            break;
        }
        if(validacao_pipe(command) == 0){
            sem_wait(sem_log);
            sprintf(mensagem, "NEW COMMAND => %s",command);
            write_log(mensagem);
            sem_post(sem_log);
        }
        else{
            sem_wait(sem_log);
            sprintf(mensagem, "WRONG COMMAND => %s",command);
            write_log(mensagem);
            sem_post(sem_log);
        }
    }   
}

int main(void){
	le_configuracoes(&gs_configuracoes);
	//listas ligadas para criar as threads
	thread_list_prt = NULL;
	thread_list_atr = NULL;
	//mutexes para essas listas
	pthread_mutex_init(&mutex_list_prt, NULL);
	pthread_mutex_init(&mutex_list_atr, NULL);
	//CV's para as listas
	pthread_cond_init(&is_prt_list_empty, NULL);
	pthread_cond_init(&is_atr_list_empty, NULL);


    //MESSAGE QUEUE
    if ((msg_q_id= msgget(IPC_PRIVATE,IPC_CREAT | 0700))==-1){
        printf("ERRO ao criar message queue\n");
    }

    //SHARED MEMORY
    if((shmid = shm_open(SHARED_MEM_NAME,   O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    if (ftruncate(shmid, sizeof(estatisticas_sistema) + gs_configuracoes.qnt_max_partidas * sizeof(voo_partida) + gs_configuracoes.qnt_max_chegadas)*sizeof(voo_partida) == -1){
        printf("Error defining size\n");
        exit(1);
    }
    //para alterar as estatisticas faz se atraves deste ponteiro   
    estatisticas = (estatisticas_sistema *) mmap(NULL, sizeof(estatisticas_sistema), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid, 0);
    
    array_voos_partida = (voo_partida *)mmap(NULL, gs_configuracoes.qnt_max_partidas * sizeof(voo_partida), PROT_WRITE  | PROT_READ, MAP_SHARED,shmid, sizeof(estatisticas_sistema));
    //size = gs_configuracoes.qnt_max_partidas

    array_voos_chegada = (voo_chegada *)mmap(NULL, gs_configuracoes.qnt_max_chegadas * sizeof(voo_chegada), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid, sizeof(estatisticas_sistema) + gs_configuracoes.qnt_max_partidas * sizeof(voo_partida));
    //size = gs_configuracoes.qnt_max_chegadas

    //SEMAFOROS
    if((sem_estatisticas = sem_open(STATS_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((sem_partidas = sem_open(DEPARTURES_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((sem_chegadas = sem_open(ARRIVALS_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((sem_log = sem_open(LOG_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    //Criacao do processo Torre de Controlo e inicio do servidor
    pid_t pid = fork();

	if(pid == 0){
		torre_controlo();
		exit(0);
	} 

    //cria o pipe
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)){
        perror("Cannot create pipe");
        exit(1);
    } 
    
    //abrir o pipe para leitura
    if ((fd_pipe = open(PIPE_NAME, 0666)) < 0) { 
        perror("Erro a abrir o pipe para leitura");
        exit(1);
    } 

    /*O gestor de simulacao vai receber
    comandos pelo pipe, le-los, criar
    as threads necessarias, por na 
    message queue para a torre de controlo
    os receber e a torre de controlo guarda
    essa informacao (sobre os voos) na 
    shared memory
    */
	gestor_simulacao();

    wait(NULL);

    //Apagar recursos
    //shared memory, pipe, message queue, semaforos, etc
    sem_unlink(LOG_SEMAPHORE);
    sem_close(sem_log);
    sem_unlink(ARRIVALS_SEMAPHORE);
    sem_close(sem_chegadas);
    sem_unlink(DEPARTURES_SEMAPHORE);
    sem_close(sem_partidas);
    sem_unlink(STATS_SEMAPHORE);
    sem_close(sem_estatisticas);
    unlink(PIPE_NAME);
    close(fd_pipe);
    exit(0);
}
