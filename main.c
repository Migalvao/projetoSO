//PARA COMPILAR: gcc -Wall -pthread main.c -o main -lrt
#include "header.h"

int msg_q_id;   //MESSAGE QUEUE 
int fd_pipe;    //PIPE
int shmid;      //SHARED MEMORY
int * array_voos_partida;
int * array_voos_chegada;

estatisticas_sistema * estatisticas;

char comando[MAX_SIZE_COMANDO];
char mensagem[MAX_SIZE_MSG];

sem_t * sem_stats;      //semaforo para estatisticas

void torre_controlo(){
	//printf("Ola sou a torre de controlo. Pid = %d\n", getpid());
    /*
    para tentar alterar estatisticas
    sem_wait(sem_stats);

    estatisticas->qualquercoisa

    sem_post(sem_stats);
    */
}

void gestor_simulacao(){
    pthread_t thread_intermedia;
    int init;
    time(&t_inicial);   //definir o tempo inicial, declarado em header.h
    write_log("Servidor iniciado");

    read(fd_pipe,comando,MAX_SIZE_COMANDO);
    if(validacao_pipe(comando, &init) == 0){
        sprintf(mensagem, "NEW COMMAND => %s",comando);
        write_log(mensagem);
        if(pthread_create(&thread_intermedia, NULL, criar_thread, &init) !=0){
            printf("Erro a criar thread\n");
        }

    }
    else{
        sprintf(mensagem, "WRONG COMMAND => %s",comando);
        write_log(mensagem);
    }
    pthread_join(thread_intermedia, NULL);
    /*
    para tentar alterar estatisticas
    sem_wait(sem_stats);

    estatisticas->qualquercoisa
    
    sem_post(sem_stats);
    */
    }


int main(void){
	le_configuracoes(&gs_configuracoes);

    //MESSAGE QUEUE
    if ((msg_q_id= msgget(IPC_PRIVATE,IPC_CREAT | 0700))==-1){
        printf("ERRO ao criar message queue\n");
    }

    //SHARED MEMORY
    if( (shmid = shm_open(SHARED_MEM_NAME,   O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    if (ftruncate(shmid, sizeof(estatisticas_sistema) + gs_configuracoes.qnt_max_partidas * sizeof(voo_partida) + gs_configuracoes.qnt_max_chegadas)*sizeof(voo_partida) == -1){
        printf("Error defining size\n");
        exit(1);
    }
    //para alterar as estatisticas faz se atraves deste ponteiro   
    estatisticas = (estatisticas_sistema *) mmap(NULL, sizeof(estatisticas_sistema), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid, 0);
    
    array_voos_partida = (int *)mmap(NULL, gs_configuracoes.qnt_max_partidas * sizeof(voo_partida), PROT_WRITE  | PROT_READ, MAP_SHARED,shmid, sizeof(estatisticas_sistema));
    //size = gs_configuracoes.qnt_max_partidas

    array_voos_chegada = (int *)mmap(NULL, gs_configuracoes.qnt_max_chegadas * sizeof(voo_chegada), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid, sizeof(estatisticas_sistema) + gs_configuracoes.qnt_max_partidas * sizeof(voo_partida));
    //size = gs_configuracoes.qnt_max_chegadas

    //SEMAFOROS
    if((sem_stats = sem_open(STATS_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
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
    unlink(PIPE_NAME);
    close(fd_pipe);
    exit(0);
}
