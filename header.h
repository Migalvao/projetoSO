﻿//Miguel Galvão-2018278986 Sofia Silva- 2018293871 

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

#define CONTROL_TOWER "/control_tower"
#define STATS_SEMAPHORE "/stats_semaphore"
#define LOG_SEMAPHORE "/log_semaphore"
#define SERVER_TERMINATED "/server_terminated"
#define TERMINATE_SERVER "/terminate_server"
#define SEND_SIGNAL "/send_signal"
#define SIGNAL_SENT "/signal_sent"
#define SHM_STATS "/shm_stats"
#define SHM_DEP "/shm_dep"
#define SHM_ARR "/shm_arr"
#define PIPE_NAME "input_pipe" 
#define MAX_SIZE_COMANDO 50
#define MAX_SIZE_MSG 80
#define SIZE_HORAS 9
#define PISTA_28L "28L"
#define PISTA_28R "28R"
#define PISTA_01L "1L"
#define PISTA_01R "1R"
#define PISTA_28L2 "28L2"
#define PISTA_28R2 "28R2"
#define PISTA_01L2 "1L2"
#define PISTA_01R2 "1R2"


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
        fuel,
        instrucao;        
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
    int id_slot_shm;
}node_chegadas;

typedef struct{
    long msg_type;
    int id_slot_shm,
        takeoff,
        eta;
}mensagens;

//variaveis globais
int pid;        //pid torre de controlo
int msg_q_id;   //MESSAGE QUEUE
int fd_pipe;    //PIPE
int shmid_stats, shmid_dep, shmid_arr;      //SHARED MEMORY
int running;
estatisticas_sistema * estatisticas;

configuracoes gs_configuracoes;
thread_prt thread_list_prt;      //lista para criar threads de partidas
thread_atr thread_list_atr;      //Lista para criar thread de aterragens

voo_partida * array_voos_partida;       //array de partidas na shm
voo_chegada * array_voos_chegada;       //array de chegadas na shm

voos_chegada fila_espera_chegadas;
voos_partida fila_espera_partidas;

pthread_cond_t is_atr_list_empty, is_prt_list_empty, check_atr, check_prt, nmr_aterragens;
pthread_t thread_criadora_partidas, thread_criadora_chegadas, thread_sinais;                        //threads Gestor de simulaçao
pthread_t thread_inicializadora, thread_msq, thread_fuel, thread_terminate, thread_holding;         //threads Torre de Controlo

time_t t_inicial;

pthread_mutex_t mutex_list_atr, mutex_list_prt;                 //Mutexes para as listas de criacao de threads
pthread_mutex_t mutex_array_atr, mutex_array_prt;               //mutexes para os arrays na shm
pthread_mutex_t mutex_fila_chegadas, mutex_fila_partidas;       //mutexes para as listas de espera

sem_t * sem_estatisticas;                   //semaforo para estatisticas    
sem_t * sem_log;                            //semaforo para o log
sem_t * enviar_sinal;                       //para enviar os sinais entre processos
sem_t * sinal_enviado;                      //para enviar os sinais entre processos
sem_t * terminar_server;
sem_t * server_terminado;
sem_t * mutex_01R_start;                          //pistas para aterrar e levantar
sem_t * mutex_01R_end;
sem_t * mutex_01L_start;
sem_t * mutex_01L_end;
sem_t * mutex_28L_start;
sem_t * mutex_28L_end;
sem_t * mutex_28R_start;
sem_t * mutex_28R_end;
char mensagem[MAX_SIZE_MSG];
char comando[MAX_SIZE_COMANDO];

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

void * recebe_msq();

int procura_slot_chegada();

int procura_slot_partida();

thread_atr adicionar_nova_atr(thread_atr thread_list, voo_chegada voo);

thread_prt adicionar_nova_prt(thread_prt thread_list, voo_partida voo);

voos_partida adicionar_fila_partidas(voos_partida lista_partidas, mensagens voo_part);

void adicionar_fila_chegadas(mensagens voo_cheg);

void adicionar_inicio(mensagens voo_cheg);

voos_partida remove_partida(voos_partida head);

void remove_chegada();

void remove_por_id(int id);

void * decrementa_fuel_eta(void * t);

void * enviar_sinal_threads(void*t);

void termination_handler(int signo);

void * wait_lists (void * t);

void * receber_comandos(void * t);

void sinal_estatisticas();

void swap();

void ordena_ETA();

void * holding(void *t);