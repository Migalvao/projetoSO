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
#define PISTA_28L "28L"
#define PISTA_28R "28R"
#define PISTA_01L "1L"
#define PISTA_01R "1R"


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
	char flight_code[10], pista[4];
	int init,
		eta,
        fuel;        
} voo_chegada;

typedef struct departure_flight{
	char flight_code[10], pista[3];
	int init,
		takeoff;    
} voo_partida;

typedef struct no_atr * thread_atr;

typedef struct no_atr
{
    thread_atr next;
    voo_chegada voo;
} node_atr;

typedef struct no_prt * thread_prt;

typedef struct no_prt
{
    thread_prt next;
    voo_partida voo;
} node_prt;

typedef struct no_fila_partida * voos_partida;

typedef struct no_fila_partida{
    voos_partida next;
    int id_slot_shm, takeoff;
}node_partidas;

typedef struct no_fila_chegada *  voos_chegada;

typedef struct no_fila_chegada{
    voos_chegada next;
    int id_slot_shm, eta;
}node_chegadas;

typedef struct{
    long msg_type;
    int id_slot_shm,
        takeoff,
        eta;
}mensagens;

//variaveis globais
int msg_q_id;   //MESSAGE QUEUE
estatisticas_sistema * estatisticas;

configuracoes gs_configuracoes;
thread_prt thread_list_prt;      //lista para criar threads de partidas
thread_atr thread_list_atr;      //Lista para criar thread de aterragens

voo_partida * array_voos_partida;       //array de partidas na shm
voo_chegada * array_voos_chegada;       //array de chegadas na shm

voos_chegada fila_espera_chegadas;
voos_partida fila_espera_partidas;

pthread_cond_t is_atr_list_empty, is_prt_list_empty, check_atr, check_prt;

time_t t_inicial;

pthread_mutex_t mutex_list_atr, mutex_list_prt;                 //Mutexes para as listas de criacao de threads
pthread_mutex_t mutex_array_atr, mutex_array_prt;               //mutexes para os arrays na shm
pthread_mutex_t mutex_28L, mutex_28R, mutex_01L, mutex_01R;     //mutexes para as pistas
pthread_mutex_t mutex_fila_chegadas, mutex_fila_partidas;       //mutexes para as listas de espera

sem_t * sem_estatisticas;       //semaforo para estatisticas    
sem_t * sem_log;                //semaforo para o log
char mensagem[MAX_SIZE_MSG];

//Funcoes
int verifica_numero(char * nmr);

void write_log(char* mensagem);

void le_configuracoes(configuracoes * configs);

void * inicializar_shm(void * t);

void * criar_thread(void * init);

int validacao_pipe(char * comando);

void * partida(void * t);

void * criar_partida(void * t);

void * chegada(void * t);

void * criar_chegada(void * t);

thread_atr adicionar_nova_atr(thread_atr thread_list, voo_chegada voo);

thread_prt adicionar_nova_prt(thread_prt thread_list, voo_partida voo);

voos_partida adicionar_fila_partidas(voos_partida lista_partidas, mensagens voo_part);

voos_chegada adicionar_fila_chegadas(voos_chegada lista_chegadas, mensagens voo_cheg);

voos_chegada adicionar_inicio(voos_chegada lista_prioritarios, mensagens voo_cheg);

voos_partida remove_partida(voos_partida head);

voos_chegada remove_chegada(voos_chegada head);

voos_chegada remove_por_id(voos_chegada head, int id);