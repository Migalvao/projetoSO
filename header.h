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

#define STATS_SEMAPHORE "/stats_semaphore"
#define SHARED_MEM_NAME "/gestor_simulacao"
#define PIPE_NAME "input_pipe" 
#define MAX_SIZE_COMANDO 50
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


void write_log(char* mensagem);

void le_configuracoes(configuracoes * configs);

void create_thread(int init, int ut);

void validacao_pipe(char* comando);
