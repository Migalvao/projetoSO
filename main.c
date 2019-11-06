#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> // include POSIX semaphores
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>   
#include <fcntl.h> 
#include <sys/types.h> 
#include <sys/wait.h> 
#include <unistd.h>
#include <errno.h>

#define STATS_SEMAPHORE "/stats_semaphore"
#define SHARED_MEM_NAME "/gestor_simulacao"

typedef struct system_stats
{
	int n_voos_criados,
		n_voos_aterrados,
		n_voos_descolados,
		n_voos_redirecionados,
		n_voos_rejeitados; 
    double tempo_medio_aterrar,     //+ ETA
        tempo_medio_descolar,
        n_medio_holdings_aterragem,
        n_medio_holdings_urgencia;
}estatisticas_sistema;

typedef struct config{
    int unidade_tempo,
        dur_descolagem,
        int_descolagem,
        dur_aterragem,
        int_aterragem,
        dur_min,
        dur_max,
        qnt_max_partidas,
        qnt_max_chegadas;
} configuracoes;


int msg_q_id;   //MESSAGE QUEUE 
int fd_pipe;    //PIPE
int shmid;      //SHARED MEMORY

sem_t * sem_stats;      //semaforo para estatisticas

configuracoes gs_configuracoes;
estatisticas_sistema * estatisticas;

void le_configuracoes(configuracoes configs ){
    char linha[200];
    char u_tempo[100], d_desc[100], i_desc[100], d_aterr[100], i_aterr[100], d_min[100], d_max[100], max_part[100], max_cheg[100];
    FILE *f = fopen("configuracoes.txt","r");
    int contador=0;


    if (f != NULL){
        while (fgets(linha, 200, f) != NULL){
            if (contador==0){ //unidade tempo
                sscanf(linha, " %[^\n]", u_tempo);
                configs.unidade_tempo= atoi(u_tempo);
                //printf(" unidade tempo %d\n", configs.unidade_tempo);
                contador++;
            }
            else if(contador==1){ //duracao dscolagem e intervalo descolagem
                sscanf(linha, "%[^,], %s", d_desc, i_desc );
                configs.dur_descolagem= atoi(d_desc);
                configs.int_descolagem= atoi(i_desc);
                //printf("duracao descolagem %d intervalo descolagem %d\n", configs.dur_descolagem, configs.int_descolagem);
                contador++;
            }
            else if (contador==2){ //duracao aterragem e intervalo aterragem
                sscanf(linha, "%[^,], %s", d_aterr, i_aterr);
                configs.dur_aterragem= atoi(d_aterr);
                configs.int_aterragem= atoi(i_aterr);
                //printf("duracao aterragem %d intervalo aterragem %d\n", configs.dur_aterragem, configs.int_aterragem);
                contador++;
            }
            else if (contador==3){
                sscanf(linha, "%[^,], %s",d_min, d_max );
                configs.dur_min=atoi(d_min);
                configs.dur_max=atoi(d_max);
                //printf("duracao minima %d duracao maxima %d\n", configs.dur_min, configs.dur_max);
                contador++;
            }
            else if (contador==4){
                sscanf(linha, " %[^\n]",max_part);
                configs.qnt_max_partidas=atoi(max_part);
                //printf("quantidade maxima de paertidas %d\n", configs.qnt_max_partidas);
                contador++;
            }
            else if (contador==5){
                sscanf(linha, " %[^\n]", max_cheg);
                configs.qnt_max_chegadas=atoi(max_cheg);
                //printf("quantidade maxima de chegadas %d\n", configs.qnt_max_chegadas);
                contador++;
            }
        }
    } else
        printf ("ERRO A ABRIR FICHEIRO!");
    fclose (f);
}

void torre_controlo(){
	printf("Ola sou a torre de controlo. Pid = %d\n", getpid());
    /*
    para tentar alterar estatisticas
    sem_wait(sem_stats);

    estatisticas->qualquercoisa

    sem_post(sem_stats);
    */
}

void gestor_simulacao(){
	printf("Ola sou o gestor de simulacao. Pid = %d\n", getpid());
    /*
    para tentar alterar estatisticas
    sem_wait(sem_stats);

    estatisticas->qualquercoisa
    
    sem_post(sem_stats);
    */
    }

int main(void){

	le_configuracoes(gs_configuracoes);

    //cria o pipe
    if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno != EEXIST)){
        printf("Erro ao criar o PIPE\n");
    } else {
        printf("->Named Pipe criado.\n");
    }

    //MESSAGE QUEUE
    if ((msg_q_id= msgget(IPC_PRIVATE,IPC_CREAT | 0700))==-1){
        printf("ERRO ao criar message queue\n");
    }else
    {
        printf("Message queue criada.\n");
    }

    //SHARED MEMORY
    if( (shmid = shm_open(SHARED_MEM_NAME,   O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    //Tamanho da shared memory -> a alterar depois
    //Para ja so tem tamanho para as estatisticas
    if (ftruncate(shmid, sizeof(estatisticas_sistema)) == -1){
        printf("Error defining size\n");
        exit(1);
    }
    //para alterar as estatisticas faz se atraves deste ponteiro   
    estatisticas = (estatisticas_sistema *) mmap(NULL, sizeof(estatisticas_sistema), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid, 0);

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

    /*O gestor de simulacao vai receber
    comandos pelo pipe, le-los, criar
    as threads necessarias, por na 
    message queue para a torre de controlo
    os receber e a torre de controlo guarda
    essa informacao (sobre os voos) na 
    shared memory
    */
	gestor_simulacao();

    //Apagar recursos
    //shared memory, pipe, message queue, semaforos, etc
    exit(0);
}