//PARA COMPILAR: gcc -Wall -pthread main.c -o main -lrt
#include "header.h"
 
int fd_pipe;    //PIPE
int shmid;      //SHARED MEMORY

char comando[MAX_SIZE_COMANDO];

void torre_controlo(){
    //a lista da fila de espera de chegadas vai ter um header node que tem em eta o numero de voos a espera
    fila_espera_chegadas = (voos_chegada)malloc(sizeof(node_chegadas));
    fila_espera_chegadas->eta = 0;
    fila_espera_partidas = NULL;
    pthread_t thread_inicializadora;
    //inicializar a shm
    pthread_create(&thread_inicializadora, NULL, inicializar_shm, NULL);
    pthread_join(thread_inicializadora, NULL);

    sem_wait(sem_log);
    sprintf(mensagem, "Torre de controlo iniciada. Pid: %d", getpid());
    write_log(mensagem);
    sem_post(sem_log);

    while(1){
        if(msgrcv(msg_q_id, &mensagem, sizeof(mensagens)-sizeof(long), -2, 0)==-1){
            printf("ERRO a receber mensagem na torre de controlo.\n");
        }
    }

}

void gestor_simulacao(){
    pthread_t thread_msq;
    char * command;
    pthread_t thread_criadora_partidas, thread_criadora_chegadas;
    time(&t_inicial);   //definir o tempo inicial, declarado em header.h
    pthread_create(&thread_criadora_partidas, NULL, criar_partida, NULL);
    pthread_create(&thread_criadora_chegadas, NULL, criar_chegada, NULL);
    sem_wait(sem_log);
    sprintf(mensagem, "Gestor de simulação iniciado. Pid: %d",getpid());
    write_log(mensagem);
    sem_post(sem_log);
    pthread_create(&thread_msq, NULL, recebe_msq,NULL);

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
	//mutexes
	pthread_mutex_init(&mutex_list_prt, NULL);
	pthread_mutex_init(&mutex_list_atr, NULL);
    pthread_mutex_init(&mutex_array_atr, NULL);
    pthread_mutex_init(&mutex_28L, NULL);
    pthread_mutex_init(&mutex_28R, NULL);
    pthread_mutex_init(&mutex_01L, NULL);
    pthread_mutex_init(&mutex_01R, NULL);
    pthread_mutex_init(&mutex_fila_chegadas, NULL);
    pthread_mutex_init(&mutex_fila_partidas, NULL);
	//CV's para as listas
	pthread_cond_init(&is_prt_list_empty, NULL);
	pthread_cond_init(&is_atr_list_empty, NULL);
    pthread_cond_init(&check_atr, NULL);
    pthread_cond_init(&check_prt, NULL);


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

	gestor_simulacao();

    wait(NULL);

    //Apagar recursos
    //shared memory, pipe, message queue, semaforos, etc
    sem_unlink(LOG_SEMAPHORE);
    sem_close(sem_log);
    sem_unlink(STATS_SEMAPHORE);
    sem_close(sem_estatisticas);
    unlink(PIPE_NAME);
    close(fd_pipe);
    exit(0);
}
