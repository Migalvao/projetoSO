//PARA COMPILAR: gcc -Wall -pthread main.c -o main -lrt
#include "header.h"
 
void torre_controlo(){
    int id1, id2;
    time_t t_atual;
    struct sigaction action;
    action.sa_handler = sinal_estatisticas;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigset_t block_signals;
    sigfillset(&block_signals);
    sigdelset(&block_signals, SIGUSR1);
    sigprocmask(SIG_BLOCK, &block_signals, NULL);

    sigaction(SIGUSR1, &action, NULL);

    //a lista da fila de espera de chegadas vai ter um header node que tem no eta o numero de voos a espera
    fila_espera_chegadas = (voos_chegada)malloc(sizeof(node_chegadas));
    fila_espera_chegadas->id_slot_shm = 0;
    fila_espera_chegadas->next = NULL;
    fila_espera_partidas = NULL;

    //inicializar a shm
    pthread_create(&thread_inicializadora, NULL, inicializar_shm, NULL);
    pthread_join(thread_inicializadora, NULL);

    pthread_create(&thread_terminate, NULL, wait_lists, NULL);

    pthread_create(&thread_msq, NULL, recebe_msq,NULL);                 //thread que controla a msg queue
    pthread_create(&thread_fuel, NULL, decrementa_fuel_eta,NULL);       //thread que decrementa o fuel a cada UT

    sem_wait(sem_log);
    sprintf(mensagem, "Torre de controlo iniciada. Pid: %d", getpid());
    write_log(mensagem);
    sem_post(sem_log);

    //ESCALONAMENTO
    
    while(1){
        if(running != 0){
            pthread_join(thread_terminate, NULL);
            exit(0);
        }
        //ARRIVALS
        pthread_mutex_lock(&mutex_array_atr);               //array smh
        pthread_mutex_lock(&mutex_fila_chegadas);           //fila espera
        if(fila_espera_chegadas->next!=NULL && array_voos_chegada[fila_espera_chegadas->next->id_slot_shm].eta <= 0){
            id1 = fila_espera_chegadas->next->id_slot_shm;
            //atualizar estatisticas
            sem_wait(sem_estatisticas);
            estatisticas->n_voos_aterrados ++;
            estatisticas->tempo_medio_aterrar = (estatisticas->tempo_medio_aterrar * (estatisticas->n_voos_aterrados - 1) + abs(array_voos_chegada[fila_espera_chegadas->next->id_slot_shm].eta)) / estatisticas->n_voos_aterrados;
            sem_post(sem_estatisticas);

            if(fila_espera_chegadas->next->next!=NULL && array_voos_chegada[fila_espera_chegadas->next->next->id_slot_shm].eta <= 0){
                id2 = fila_espera_chegadas->next->next->id_slot_shm;
                //atualizar estatisticas
                sem_wait(sem_estatisticas);
                estatisticas->n_voos_aterrados ++;
                estatisticas->tempo_medio_aterrar = (estatisticas->tempo_medio_aterrar * (estatisticas->n_voos_aterrados - 1) + abs(array_voos_chegada[fila_espera_chegadas->next->id_slot_shm].eta)) / estatisticas->n_voos_aterrados;
                sem_post(sem_estatisticas);
            } else {
                id2 = -1;
            }
            pthread_mutex_unlock(&mutex_fila_chegadas);     //fila espera

            //dar ordem para aterrar e indicar a pista
            array_voos_chegada[id1].instrucao= 1;
            strcpy(array_voos_chegada[id1].pista, PISTA_28L);

            if(id2 != -1){
                array_voos_chegada[id2].instrucao= 1;
                strcpy(array_voos_chegada[id2].pista, PISTA_28R);
            }

            sem_post(enviar_sinal);     //enviar notificaçao para as threads

            sem_wait(sinal_enviado);    //esperar pela confirmacao
            pthread_mutex_unlock(&mutex_array_atr);     //array shm

            sem_post(mutex_28L_start);           //pista
            if(id2 != -1){
                sem_post(mutex_28R_start);
            }

            pthread_mutex_lock(&mutex_fila_chegadas);
            remove_chegada();
            pthread_mutex_unlock(&mutex_fila_chegadas);

            if(id2 != -1){
                pthread_mutex_lock(&mutex_fila_chegadas);
                remove_chegada();
                pthread_mutex_unlock(&mutex_fila_chegadas);
            }

            //esperar pelas pistas ficarem livres
            sem_wait(mutex_28L_end);
            if(id2 != -1){
                sem_wait(mutex_28R_end);
            }
        } else{
            pthread_mutex_unlock(&mutex_fila_chegadas);
            pthread_mutex_unlock(&mutex_array_atr);
        }

        //DEPARTURES
        t_atual = ((time(NULL) - t_inicial) * 1000) / gs_configuracoes.unidade_tempo;
        pthread_mutex_lock(&mutex_fila_partidas);
        if(fila_espera_partidas!=NULL && (fila_espera_partidas->takeoff <= t_atual)){
            id1 = fila_espera_partidas->id_slot_shm;    
            if(fila_espera_partidas->next !=NULL && (fila_espera_partidas->next->takeoff <= t_atual)){
                id2= fila_espera_partidas->next->id_slot_shm;
            }  else
                id2=-2;            
            pthread_mutex_unlock(&mutex_fila_partidas);

            pthread_mutex_lock(&mutex_array_prt);
            if(id2!=-2){
                array_voos_partida[id2].takeoff= 0;
            }
            array_voos_partida[id1].takeoff= 0;
            sem_wait(sem_estatisticas);
            if (id2 != -2){
                estatisticas->n_voos_descolados++;
                estatisticas->tempo_medio_descolar= (estatisticas->tempo_medio_descolar * (estatisticas->n_voos_descolados-1) + (t_atual - fila_espera_partidas->next->takeoff)) / (estatisticas->n_voos_descolados); 
            }
            estatisticas->n_voos_descolados++;
            estatisticas->tempo_medio_descolar= (estatisticas->tempo_medio_descolar * (estatisticas->n_voos_descolados-1) + (t_atual - fila_espera_partidas->takeoff)) / (estatisticas->n_voos_descolados); 
            sem_post(sem_estatisticas);
            if(id2!=-2){
                strcpy(array_voos_partida[id2].pista, PISTA_01R);
            }
            strcpy(array_voos_partida[id1].pista, PISTA_01L);
            
            sem_post(enviar_sinal);     //enviar sinal
            sem_wait(sinal_enviado);    //esperar pela confirmacao
            pthread_mutex_unlock(&mutex_array_prt);

            sem_post(mutex_01L_start);
            sem_post(mutex_01R_start);
            
            pthread_mutex_lock(&mutex_fila_partidas);
            if(id2!=-2){
                remove_partida(fila_espera_partidas);
            }
            fila_espera_partidas = remove_partida(fila_espera_partidas);
            pthread_mutex_unlock(&mutex_fila_partidas);
            printf("3\n");
            //esperar pela aterragem acabar
            sem_wait(mutex_01L_end);
            if(id2!=-2){
                sem_wait(mutex_01R_end);
            }
        }else{
            pthread_mutex_unlock(&mutex_fila_partidas);     
        }
        usleep(gs_configuracoes.unidade_tempo*1000);
    }
}

void gestor_simulacao(){
    struct sigaction action;
    action.sa_handler = termination_handler;
    sigfillset(&action.sa_mask);
    action.sa_flags = 0;

    sigset_t block_signals;
    sigfillset(&block_signals);
    sigdelset(&block_signals, SIGINT);
    sigprocmask(SIG_BLOCK, &block_signals, NULL);

    sigaction(SIGINT, &action, NULL);

    char * command;
    pthread_create(&thread_criadora_partidas, NULL, criar_partida, NULL);
    pthread_create(&thread_criadora_chegadas, NULL, criar_chegada, NULL);
    pthread_create(&thread_sinais, NULL, enviar_sinal_threads,NULL);        //thread que recebe um sinal de outro processo e o transmite para as threads voo

    sem_wait(sem_log);
    sprintf(mensagem, "Gestor de simulação iniciado. Pid: %d",getpid());
    write_log(mensagem);
    sem_post(sem_log);

    while(1){
        read(fd_pipe,comando,MAX_SIZE_COMANDO);
        command = strtok(comando, "\n");
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
    pthread_mutex_init(&mutex_array_prt, NULL);
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
    if((shmid_stats = shm_open(SHM_STATS, O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    if (ftruncate(shmid_stats, sizeof(estatisticas_sistema)) == -1){
        printf("Error defining size\n");
        exit(1);
    }

    if((shmid_dep = shm_open(SHM_DEP, O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    if (ftruncate(shmid_dep, gs_configuracoes.qnt_max_partidas * sizeof(voo_partida)) == -1){
        printf("Error defining size\n");
        exit(1);
    }

    if((shmid_arr = shm_open(SHM_ARR, O_RDWR | O_CREAT ,0777)) == -1){
        printf("Error creating memory\n");
        exit(1);
    }

    if (ftruncate(shmid_arr, gs_configuracoes.qnt_max_chegadas *sizeof(voo_chegada)) == -1){
        printf("Error defining size\n");
        exit(1);
    }
    //para alterar as estatisticas faz se atraves deste ponteiro   
    estatisticas = (estatisticas_sistema *) mmap(NULL, sizeof(estatisticas_sistema), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid_stats, 0);
    
    array_voos_partida = (voo_partida *)mmap(NULL, gs_configuracoes.qnt_max_partidas * sizeof(voo_partida), PROT_WRITE  | PROT_READ, MAP_SHARED,shmid_dep, 0);

    array_voos_chegada = (voo_chegada *)mmap(NULL, gs_configuracoes.qnt_max_chegadas * sizeof(voo_chegada), PROT_WRITE  | PROT_READ, MAP_SHARED, shmid_arr, 0);

    //SEMAFOROS
    if((sem_estatisticas = sem_open(STATS_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((sem_log = sem_open(LOG_SEMAPHORE, O_CREAT, 0777, 1)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((enviar_sinal = sem_open(SEND_SIGNAL, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }


    if((sinal_enviado = sem_open(SIGNAL_SENT, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((terminar_server = sem_open(TERMINATE_SERVER, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((server_terminado = sem_open(SERVER_TERMINATED, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_01L_start = sem_open(PISTA_01L, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_01R_start = sem_open(PISTA_01R, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_28L_start = sem_open(PISTA_28L, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_28R_start = sem_open(PISTA_28R, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_01L_end = sem_open(PISTA_01L2, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_01R_end = sem_open(PISTA_01R2, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_28L_end = sem_open(PISTA_28L2, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    if((mutex_28R_end = sem_open(PISTA_28R2, O_CREAT, 0777, 0)) == SEM_FAILED){
        printf("Error starting semaphore\n");
        exit(1);
    }

    time(&t_inicial);       //definir o tempo inicial, declarado em header.h
    pid = fork();

    running = 0;
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
}
