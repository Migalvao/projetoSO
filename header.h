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
#include <sys/msg.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>

#define STATS_SEMAPHORE "/stats_semaphore"
#define LOG_SEMAPHORE "/log_semaphore"
#define ARRIVALS_SEMAPHORE "/arrivals_semaphore"
#define DEPARTURES_SEMAPHORE "/departures_semaphore"
#define SHARED_MEM_NAME "/gestor_simulacao"
#define PIPE_NAME "input_pipe" 
#define MAX_SIZE_COMANDO 50
#define MAX_SIZE_MSG 80
#define SIZE_HORAS 9


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

typedef struct arrival_flight{
	char flight_code[10];
	int init,
		eta,
        fuel;        
} voo_chegada;

typedef struct departure_flight{
	char flight_code[10];
	int init,
		takeoff;    
} voo_partida;

typedef struct thread_no * thread;

typedef struct thread_no
{
    thread next;
    pthread_t thread_id;
} thread_node;

//variaveis globais
configuracoes gs_configuracoes;
//thread thread_list = NULL;
time_t t_inicial;
sem_t * sem_estatisticas;       //semaforo para estatisticas
sem_t * sem_chegadas;           //semaforo para chegadas
sem_t * sem_partidas;           //semaforo para partidas          
sem_t * sem_log;                //semaforo para o log
char mensagem[MAX_SIZE_MSG];

//Funcoes
void write_log(char* mensagem);

void le_configuracoes(configuracoes * configs);

void * criar_thread(void * init);

int validacao_pipe(char * comando);

void * partida(void * t);

void * criar_partida(void * t);

void * chegada(void * t);

void * criar_chegada(void * t);
