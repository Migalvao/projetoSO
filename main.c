#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> // include POSIX semaphores
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>   
#include <fcntl.h> 
#include <sys/types.h> 
#include <sys/wait.h> 
#include <unistd.h>
#include <errno.h>

typedef struct system_stats
{
	int n_voos_criados = 0,
		n_voos_aterrados = 0,
		n_voos_descolados = 0,
		n_voos_redirecionados = 0,
		n_voos_rejeitados = 0; 
}estatisticas_sistema;

void torre_controlo(){
	printf("Ola sou a torre de controlo. Pid = %d\n", getpid());
}

void gestor_simulacao(){
	printf("Ola sou o gestor de simulacao. Pid = %d\n", getpid());
}

int main(void){
	pid_t pid = fork();

	if(pid == 0){
		torre_controlo();
		exit(0);
	} 

	else {
		gestor_simulacao();
		exit(0);
	}



}