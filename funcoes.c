#include "header.h"

int verifica_numero(char * nmr){
    char * digito = nmr;
    while((*digito) != '\0'){
        if((*digito) < '0' || (*digito) > '9'){
            return 1;
        } 
        else 
            digito ++;
    }
    return 0;
}

void write_log(char * mensagem){
    FILE  *f =fopen("log.txt","a");
    time_t tempo;
    struct tm* estrutura_temp;
    char horas[SIZE_HORAS];

    time(&tempo);
    estrutura_temp = localtime(&tempo);
    strftime(horas,SIZE_HORAS,"%H:%M:%S", estrutura_temp);
    fprintf(f,"%s %s\n", horas, mensagem);
    printf("%s %s\n", horas, mensagem);

    fclose(f);
}

void le_configuracoes(configuracoes * configs ){
    char linha[200];
    char u_tempo[100], d_desc[100], i_desc[100], d_aterr[100], i_aterr[100], d_min[100], d_max[100], max_part[100], max_cheg[100];
    FILE *f = fopen("configuracoes.txt","r");
    int contador=0;

    if (f != NULL){
        while (fgets(linha, 200, f) != NULL){
            if (contador==0){ //unidade tempo
                sscanf(linha, " %[^\n]", u_tempo);
                configs->unidade_tempo= atoi(u_tempo);
                //printf(" unidade tempo %d\n", configs->unidade_tempo);
                contador++;
            }
            else if(contador==1){ //duracao dscolagem e intervalo descolagem
                sscanf(linha, "%[^,], %s", d_desc, i_desc );
                configs->dur_descolagem= atoi(d_desc);
                configs->int_descolagem= atoi(i_desc);
                //printf("duracao descolagem %d intervalo descolagem %d\n", configs->dur_descolagem, configs->int_descolagem);
                contador++;
            }
            else if (contador==2){ //duracao aterragem e intervalo aterragem
                sscanf(linha, "%[^,], %s", d_aterr, i_aterr);
                configs->dur_aterragem= atoi(d_aterr);
                configs->int_aterragem= atoi(i_aterr);
                //printf("duracao aterragem %d intervalo aterragem %d\n", configs->dur_aterragem, configs->int_aterragem);
                contador++;
            }
            else if (contador==3){
                sscanf(linha, "%[^,], %s",d_min, d_max );
                configs->dur_min=atoi(d_min);
                configs->dur_max=atoi(d_max);
                //printf("duracao minima %d duracao maxima %d\n", configs->dur_min, configs->dur_max);
                contador++;
            }
            else if (contador==4){
                sscanf(linha, " %[^\n]",max_part);
                configs->qnt_max_partidas=atoi(max_part);
                //printf("quantidade maxima de paertidas %d\n", configs->qnt_max_partidas);
                contador++;
            }
            else if (contador==5){
                sscanf(linha, " %[^\n]", max_cheg);
                configs->qnt_max_chegadas=atoi(max_cheg);
                //printf("quantidade maxima de chegadas %d\n", configs->qnt_max_chegadas);
                contador++;
            }
        }
    } else
        printf ("ERRO A ABRIR FICHEIRO!");
    fclose (f);
}

void * inicializar_shm(void * t){
    //inicializar o array de partidas
    pthread_mutex_lock(&mutex_array_prt);   
    for(int i=0; i < gs_configuracoes.qnt_max_partidas; i++){
        array_voos_partida[i].init = -1;
    }
    pthread_mutex_unlock(&mutex_array_prt);

    //inicializar o array de chegadas
    pthread_mutex_lock(&mutex_array_atr);
    for(int i=0; i < gs_configuracoes.qnt_max_chegadas; i++){
        array_voos_chegada[i].init = -1;
        array_voos_chegada[i].fuel = -1;
    }
    pthread_mutex_unlock(&mutex_array_atr);

    sem_wait(sem_estatisticas);
    estatisticas->n_voos_criados = 0;
    estatisticas->n_voos_aterrados = 0;
    estatisticas->n_voos_descolados = 0;
    estatisticas->n_voos_rejeitados = 0;
    estatisticas->n_voos_redirecionados = 0;
    estatisticas->tempo_medio_aterrar = 0;
    estatisticas->tempo_medio_descolar = 0;
    estatisticas->n_medio_holdings_aterragem = 0;
    estatisticas->n_medio_holdings_urgencia = 0;
    sem_post(sem_estatisticas);

    pthread_exit(NULL);
}

void * partida(void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    time_t t_criacao = ((time(NULL) - t_inicial) * 1000)/gs_configuracoes.unidade_tempo;
    voo_partida * dados_partida = (voo_partida *)t;
    mensagens msg;

    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut,takeoff = %d", dados_partida->flight_code, t_criacao, dados_partida->takeoff);
    write_log(mensagem);
    sem_post(sem_log);

    sem_wait(sem_estatisticas);
    estatisticas->n_voos_criados ++;
    sem_post(sem_estatisticas);

    msg.id_slot_shm = -1;
    msg.takeoff = dados_partida->takeoff;
    msg.eta = -1;
    msg.msg_type = 2;

    if(msgsnd(msg_q_id, &msg, sizeof(mensagens)-sizeof(long), 0) == -1){
            printf("Erro a enviar msg para a Torre de Controlo\n");
    }

    //msg_type 3 indica que é uma resposta da Torre de Controlo 
    if(msgrcv(msg_q_id, &msg, sizeof(mensagens)-sizeof(long), 3, 0) == -1){
            printf("Erro a receber a resposta da Torre de Controlo\n");
    }

    if(msg.id_slot_shm == -1){
        sem_wait(sem_log);
        sprintf(mensagem, "%s FLIGHT REJECTED => SYSTEM FULL", dados_partida->flight_code);
        write_log(mensagem);
        sem_post(sem_log);

        sem_wait(sem_estatisticas);
        estatisticas->n_voos_rejeitados ++;
        sem_post(sem_estatisticas);

        free(dados_partida);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex_array_prt);
    array_voos_partida[msg.id_slot_shm].takeoff = dados_partida->takeoff;
    array_voos_partida[msg.id_slot_shm].init = dados_partida->init;
    strcpy(array_voos_partida[msg.id_slot_shm].flight_code, dados_partida->flight_code);
    pthread_mutex_unlock(&mutex_array_prt);

    printf("%s Guardei os meus dados de partida no slot %d\n", dados_partida->flight_code, msg.id_slot_shm);

    pthread_mutex_lock(&mutex_array_prt);
    while(array_voos_partida[msg.id_slot_shm].takeoff > 0){
        pthread_cond_wait(&check_prt, &mutex_array_prt);
    }

    //Thread vai iniciar descolagem
    array_voos_partida[msg.id_slot_shm].takeoff = -1;
    array_voos_partida[msg.id_slot_shm].init = -1;
    strcpy(dados_partida->pista, array_voos_partida[msg.id_slot_shm].pista);
    pthread_mutex_unlock(&mutex_array_prt);

    if(strcmp(dados_partida->pista, PISTA_01L) == 0)
        pthread_mutex_lock(&mutex_01L);
    else
        pthread_mutex_lock(&mutex_01R);

    sem_wait(sem_log);
    sprintf(mensagem, "%s DEPARTURE %s started", dados_partida->flight_code, dados_partida->pista);
    write_log(mensagem);
    sem_post(sem_log);

    usleep(gs_configuracoes.dur_descolagem * gs_configuracoes.unidade_tempo * 1000);

    sem_wait(sem_log);
    sprintf(mensagem, "%s DEPARTURE %s concluded", dados_partida->flight_code, dados_partida->pista);
    write_log(mensagem);
    sem_post(sem_log);

    usleep(gs_configuracoes.int_descolagem * gs_configuracoes.unidade_tempo * 1000);

    if(strcmp(dados_partida->pista, PISTA_01L) == 0)
        pthread_mutex_unlock(&mutex_01L);
    else
        pthread_mutex_unlock(&mutex_01R);

    sem_wait(sem_estatisticas);
    estatisticas->n_voos_descolados ++;
    sem_post(sem_estatisticas);

    free(dados_partida);
    pthread_exit(NULL);
}

void * criar_partida(void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    pthread_t thread_voo;
    voo_partida * dados_partida;
    time_t t_atual;        //em ut's
    while(1){

        pthread_mutex_lock(&mutex_list_prt);
        while(thread_list_prt == NULL){
            if(running != 0){
                printf("c\n");
                pthread_mutex_unlock(&mutex_list_prt);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&is_prt_list_empty, &mutex_list_prt);
        }
        t_atual = ((time(NULL) - t_inicial) * 1000) / gs_configuracoes.unidade_tempo;       //em ut's

        if(t_atual < thread_list_prt->voo.init){
            usleep(gs_configuracoes.unidade_tempo);
        } else {
            thread_prt atual = thread_list_prt;
            while(thread_list_prt != NULL && t_atual == thread_list_prt->voo.init){
                dados_partida = (voo_partida *)malloc(sizeof(voo_partida));
                strcpy(dados_partida->flight_code, thread_list_prt->voo.flight_code);
                dados_partida->init = thread_list_prt->voo.init;
                dados_partida->takeoff = thread_list_prt->voo.takeoff;
                pthread_create(&thread_voo, NULL, partida, dados_partida);
                //Remove da lista o que ja foi criado e passa para o seguinte
                atual = thread_list_prt->next;
                free(thread_list_prt);
                thread_list_prt = atual;
            }
        }
        pthread_mutex_unlock(&mutex_list_prt);
    }
}

void * chegada(void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    time_t t_criacao = ((time(NULL) - t_inicial) * 1000)/gs_configuracoes.unidade_tempo;    //em ut's
    time_t t_atual;
    voo_chegada * dados_chegada = (voo_chegada *)t;
    mensagens msg;

    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut, eta = %d ut,fuel = %d", dados_chegada->flight_code, t_criacao, dados_chegada->eta, dados_chegada->fuel);
    write_log(mensagem);
    sem_post(sem_log);   

    sem_wait(sem_estatisticas);
    estatisticas->n_voos_criados ++;
    sem_post(sem_estatisticas);

    msg.id_slot_shm = -1;
    msg.takeoff = -1;
    msg.eta = dados_chegada->eta;
    //se for urgente, msg_type = 1
    if(dados_chegada->fuel > (4 + dados_chegada->eta + gs_configuracoes.dur_aterragem))
        msg.msg_type = 2;
    else 
        msg.msg_type = 1;

    if(msgsnd(msg_q_id, &msg, sizeof(mensagens)-sizeof(long), 0) == -1){
            printf("Erro a enviar msg para a Torre de Controlo\n");
    }

    if(msg.msg_type == 1){
        sem_wait(sem_log);
        sprintf(mensagem, "%s EMERGENCY LANDING REQUESTED", dados_chegada->flight_code);
        write_log(mensagem);
        sem_post(sem_log);
    }

    //msg_type 3 indica que é uma resposta da Torre de Controlo 
    if(msgrcv(msg_q_id, &msg, sizeof(mensagens)-sizeof(long), 3, 0) == -1){
            printf("Erro a receber a resposta da Torre de Controlo\n");
    }

    if(msg.id_slot_shm == -1){
        sem_wait(sem_log);
        sprintf(mensagem, "%s FLIGHT REJECTED => SYSTEM FULL", dados_chegada->flight_code);
        write_log(mensagem);
        sem_post(sem_log);

        sem_wait(sem_estatisticas);
        estatisticas->n_voos_rejeitados ++;
        sem_post(sem_estatisticas);

        free(dados_chegada);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex_array_atr);
    array_voos_chegada[msg.id_slot_shm].eta = dados_chegada->eta;
    array_voos_chegada[msg.id_slot_shm].fuel = dados_chegada->fuel;
    array_voos_chegada[msg.id_slot_shm].init = dados_chegada->init;
    strcpy(array_voos_chegada[msg.id_slot_shm].flight_code, dados_chegada->flight_code);
    pthread_mutex_unlock(&mutex_array_atr);

    printf("%s Guardei os meus dados de chegada no slot %d\n", dados_chegada->flight_code,msg.id_slot_shm);

    // && array_voos_chegada[msg.id_slot_shm].eta > 0
    pthread_mutex_lock(&mutex_array_atr);
    while(array_voos_chegada[msg.id_slot_shm].fuel > 0){
        pthread_cond_wait(&check_atr, &mutex_array_atr); 
    }

    if(array_voos_chegada[msg.id_slot_shm].fuel == 0){
        //thread vai terminar porque o voo foi redirecionado
        array_voos_chegada[msg.id_slot_shm].fuel = -1;
        array_voos_chegada[msg.id_slot_shm].init = -1;
        pthread_mutex_unlock(&mutex_array_atr);

        sem_wait(sem_log);
        sprintf(mensagem, "%s LEAVING TO OTHER AIRPORT => FUEL = 0", dados_chegada->flight_code);
        write_log(mensagem);
        sem_post(sem_log);

        sem_wait(sem_estatisticas);
        estatisticas->n_voos_redirecionados ++;
        sem_post(sem_estatisticas);

        free(dados_chegada);
        pthread_exit(NULL);

    } 
    //MUDAR O IF!
    else if(NULL){
        //Vai iniciar aterragem
        array_voos_chegada[msg.id_slot_shm].eta = -1;
        array_voos_chegada[msg.id_slot_shm].init = -1;
        strcpy(dados_chegada->pista, array_voos_chegada[msg.id_slot_shm].pista);

        pthread_mutex_unlock(&mutex_array_atr);

        if(strcmp(dados_chegada->pista, PISTA_28L) == 0)
            pthread_mutex_lock(&mutex_28L);
        else
            pthread_mutex_lock(&mutex_28R);

        sem_wait(sem_log);
        sprintf(mensagem, "%s LANDING %s started", dados_chegada->flight_code, dados_chegada->pista);
        write_log(mensagem);
        sem_post(sem_log);

        usleep(gs_configuracoes.dur_aterragem * gs_configuracoes.unidade_tempo * 1000);

        sem_wait(sem_log);
        sprintf(mensagem, "%s LANDING %s concluded", dados_chegada->flight_code, dados_chegada->pista);
        write_log(mensagem);
        sem_post(sem_log);

        usleep(gs_configuracoes.int_aterragem * gs_configuracoes.unidade_tempo * 1000);

        if(strcmp(dados_chegada->pista, PISTA_28L) == 0)
            pthread_mutex_unlock(&mutex_28L);
        else
            pthread_mutex_unlock(&mutex_28R);

        sem_wait(sem_estatisticas);
        estatisticas->n_voos_aterrados ++;
        sem_post(sem_estatisticas);

        free(dados_chegada);
        pthread_exit(NULL);
    }
}

void * criar_chegada(void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    pthread_t thread_voo;
    voo_chegada * dados_chegada;
    time_t t_atual;        //em ut's
    while(1){

        pthread_mutex_lock(&mutex_list_atr);
        while(thread_list_atr == NULL){
            if(running != 0){
                printf("s\n");
                pthread_mutex_unlock(&mutex_list_atr);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&is_atr_list_empty, &mutex_list_atr);
        }
        t_atual = ((time(NULL) - t_inicial) * 1000) / gs_configuracoes.unidade_tempo;        //em ut's

        if(t_atual < thread_list_atr->voo.init){
            pthread_mutex_unlock(&mutex_list_atr);
            usleep(gs_configuracoes.unidade_tempo * 1000);
        } else {
            thread_atr atual = thread_list_atr;
            while(thread_list_atr != NULL && t_atual == thread_list_atr->voo.init){
                dados_chegada = (voo_chegada *)malloc(sizeof(voo_chegada));
                strcpy(dados_chegada->flight_code, thread_list_atr->voo.flight_code);
                dados_chegada->init = thread_list_atr->voo.init;
                dados_chegada->eta = thread_list_atr->voo.eta;
                dados_chegada->fuel = thread_list_atr->voo.fuel;
                pthread_create(&thread_voo, NULL, chegada, dados_chegada);
                //Remove da lista o que ja foi criado e passa para o seguinte
                atual = thread_list_atr->next;
                free(thread_list_atr);
                thread_list_atr = atual;
            }
            pthread_mutex_unlock(&mutex_list_atr);
        }
    }
}

int validacao_pipe(char * comando){
    pthread_t thread_intermedia;
    char delimitador[]= " ";
    char *token;
    char copia_comando[MAX_SIZE_COMANDO];
    time_t t_atual = (time(NULL) - t_inicial) * 1000;  
    voo_chegada chegada;
    voo_chegada * dados_chegada;
    voo_partida partida;
    voo_partida * dados_partida;

    
    strcpy(copia_comando, comando);
    token = strtok(copia_comando, delimitador);

    while(token!=NULL){
        
        if (strcmp(token,"DEPARTURE")==0){
            token =strtok(NULL, delimitador);
            strcpy(partida.flight_code, token);
            //printf("%s\n",partida.flight_code);
            token = strtok(NULL, delimitador);
            if (strcmp(token,"init:")==0){
                token = strtok(NULL, delimitador);
                if(verifica_numero(token) == 1)
                    return 1;
                partida.init=atoi(token);
                if ((partida.init * gs_configuracoes.unidade_tempo) <= t_atual)
                    return 1;
                //printf("%d\n",partida.init);
                token = strtok(NULL, delimitador);
                if (strcmp(token,"takeoff:")==0){
                    token = strtok(NULL, delimitador);
                    if(verifica_numero(token) == 1)
                        return 1;
                    partida.takeoff=atoi(token);
                    if(partida.takeoff < partida.init)
                        return 1;
                    //printf("%d\n",partida.takeoff);
                    token = strtok(NULL, delimitador);
                    if(token == NULL){
                        pthread_mutex_lock(&mutex_list_prt);
                        thread_list_prt = adicionar_nova_prt(thread_list_prt, partida);
                        pthread_cond_signal(&is_prt_list_empty);
                        pthread_mutex_unlock(&mutex_list_prt);
                        return 0;
                    }
		        }              
            }
        }

        else if (strcmp(token,"ARRIVAL")==0){
            token=strtok(NULL, delimitador);
            strcpy(chegada.flight_code, token);
            //printf("%s\n", chegada.flight_code);
            token= strtok(NULL,delimitador);
            if(strcmp(token,"init:")==0){
                token= strtok(NULL,delimitador);
                if(verifica_numero(token) == 1)
                    return 1;
                chegada.init=atoi(token);
                if ((chegada.init * gs_configuracoes.unidade_tempo) <= t_atual)
                    return 1;
                //printf("%d\n", chegada.init);
                token= strtok(NULL,delimitador);
                if (strcmp(token,"eta:")==0){
                    token=strtok(NULL, delimitador);
                    if(verifica_numero(token) == 1)
                        return 1;
                    chegada.eta=atoi(token);
                    //printf("%d\n", chegada.eta);
                    token= strtok(NULL,delimitador);
                    if (strcmp(token, "fuel:")==0){
                        token=strtok(NULL, delimitador);
                        if(verifica_numero(token) == 1)
                            return 1;
                        chegada.fuel=atoi(token);
                        if(chegada.fuel < chegada.eta)
                            return 1;
                        //printf("%d\n", chegada.fuel);
                        token= strtok(NULL,delimitador);
                        if(token == NULL){
                            pthread_mutex_lock(&mutex_list_atr);
                            thread_list_atr = adicionar_nova_atr(thread_list_atr, chegada);
                            pthread_cond_signal(&is_atr_list_empty);
                            pthread_mutex_unlock(&mutex_list_atr);
                            return 0;
                        }
                    }
                }
            }
        }   
    return 1;
    }
}

thread_atr adicionar_nova_atr(thread_atr thread_list, voo_chegada voo){
    thread_atr novo_voo_atr = (thread_atr)malloc(sizeof(node_atr));
    novo_voo_atr->next = NULL;
    novo_voo_atr->voo = voo;

    if(thread_list == NULL){
        thread_list = novo_voo_atr;
        return thread_list;
    } else {
        thread_atr atual = thread_list;
        while (atual->next != NULL) {
            if (atual->next->voo.init >= novo_voo_atr->voo.init) {
                atual = atual->next;
            } else {
                novo_voo_atr->next = atual->next;
                atual->next = novo_voo_atr;
                return thread_list;
            }
        }
        atual->next = novo_voo_atr;
        return thread_list;
    }
}
thread_prt adicionar_nova_prt(thread_prt thread_list, voo_partida voo){
    //Criar o no
    thread_prt novo_voo_prt = (thread_prt)malloc(sizeof(node_prt));
    novo_voo_prt->next = NULL;
    novo_voo_prt->voo = voo;

    if(thread_list == NULL){
        thread_list = novo_voo_prt;
        return thread_list;
    } else {
        thread_prt atual = thread_list;
        while (atual->next != NULL) {
            if (atual->next->voo.init >= novo_voo_prt->voo.init) {
                atual = atual->next;
            } else {
                novo_voo_prt->next = atual->next;
                atual->next = novo_voo_prt;
                return thread_list;
            }
        }
        atual->next = novo_voo_prt;
        return thread_list;
    }
}
voos_partida adicionar_fila_partidas(voos_partida lista_partidas, mensagens voo_part){
    voos_partida nova_partida= (voos_partida)malloc(sizeof(node_partidas));
    nova_partida->next = NULL;
    nova_partida->id_slot_shm = voo_part.id_slot_shm;
    nova_partida->takeoff = voo_part.takeoff;

    if(lista_partidas==NULL){
        lista_partidas= nova_partida;
        return lista_partidas;
    }
    else{
        voos_partida atual= lista_partidas;
        while(atual->next!=NULL){
                if(atual->next->takeoff >= nova_partida->takeoff){
                    atual=atual->next;
                }else{
                    nova_partida->next= atual->next;
                    atual->next= nova_partida;
                    return lista_partidas;
                }
        }
        atual->next=nova_partida;
        return lista_partidas;
    }
}

void adicionar_fila_chegadas(mensagens voo_cheg){
    //a lista_chegadas tem um header node cujo valor de eta é o numero de nos na lista
    voos_chegada nova_chegada= (voos_chegada)malloc(sizeof(node_chegadas));
    nova_chegada->next= NULL;
    nova_chegada->id_slot_shm = voo_cheg.id_slot_shm;
    nova_chegada->eta = voo_cheg.eta;

    if(fila_espera_chegadas->next == NULL){
        fila_espera_chegadas->next = nova_chegada;
        fila_espera_chegadas->eta = 1;
        return;
    }
    else{
        voos_chegada atual= fila_espera_chegadas->next;
        while(atual->next!=NULL){
            if(atual->next->eta >= nova_chegada->eta){
                atual=atual->next;
            }else{
                nova_chegada->next=atual->next;
                atual->next=nova_chegada;
                fila_espera_chegadas->eta ++;
                return;
            }
        }
        atual->next=nova_chegada;
        fila_espera_chegadas->eta ++;
        return;
    }
}

void adicionar_inicio(mensagens voo_cheg){
    //adicionar voo marcado como urgente, ou seja, inserir no inicio da fila/lista
    voos_chegada prioritario= (voos_chegada)malloc(sizeof(node_chegadas));
    prioritario->next=NULL;
    prioritario->id_slot_shm = voo_cheg.id_slot_shm;
    prioritario->eta = voo_cheg.eta;

    if(fila_espera_chegadas->next == NULL){
        fila_espera_chegadas->next = prioritario;
    }
    else{
        prioritario->next=fila_espera_chegadas->next;
        fila_espera_chegadas->next=prioritario; 
    }
    fila_espera_chegadas->eta ++;
    return;
}

voos_partida remove_partida(voos_partida head){
    voos_partida aux = head;
    head = head->next;
    free(aux);
    return head;
}

void remove_chegada(voos_chegada head){
    voos_chegada aux = head->next;
    head->next= head->next->next;
    head->eta --;
    free(aux);
    return;
}

void remove_por_id(int id){
    voos_chegada atual = fila_espera_chegadas->next;

    if(id == atual->id_slot_shm && atual->next == NULL){
        //no caso de ser logo o primeiro E a lista so ter esse elemento
        fila_espera_chegadas->next == NULL;
        free(atual);
        return;
    }

    voos_chegada anterior = atual;
    atual = atual->next;

    while(atual != NULL){
        if(id == atual->id_slot_shm){
            anterior->next = atual->next;
            free(atual);
            fila_espera_chegadas->eta --;
            return;       
        }
        anterior = atual;
        atual= atual->next;
    }
    printf("Erro: id nao encontrado\n");
    return;
}


int procura_slot_chegadas(){
    for(int i=0; i < gs_configuracoes.qnt_max_chegadas;i++){

        if(array_voos_chegada[i].init== -1)
            return i;
    }
    return -1;
}

int procura_slot_partidas(){
    for(int i=0; i<gs_configuracoes.qnt_max_partidas;i++){
        if(array_voos_partida[i].init==-1)
        return i;
    }
    return -1;
}

void * recebe_msq(void* t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    mensagens voo;

    while(1){
        if(msgrcv(msg_q_id, &voo, sizeof(mensagens)-sizeof(long), -2, 0)==-1)
            printf("ERRO a receber mensagem na torre de controlo.\n");
        if(voo.takeoff==-1){
            if(voo.msg_type==1){
                if((voo.id_slot_shm= procura_slot_chegadas()) != -1)
                    adicionar_inicio(voo);

                voo.msg_type=3;
                if(msgsnd(msg_q_id, &voo, sizeof(mensagens)-sizeof(long),0)==-1){
                    printf("ERRO a enviar mensagem.\n");
                }
            }
            else if(voo.msg_type==2){
                if((voo.id_slot_shm= procura_slot_chegadas()) != -1)
                    adicionar_fila_chegadas(voo);
                voo.msg_type=3;
                if(msgsnd(msg_q_id, &voo, sizeof(mensagens)-sizeof(long),0)==-1){
                    printf("ERRO a enviar mensagem.\n");
                }
            }
        }
        else{
            if((voo.id_slot_shm=procura_slot_partidas()) != -1)
                adicionar_fila_partidas(fila_espera_partidas, voo);

            voo.msg_type=3;
            if(msgsnd(msg_q_id, &voo, sizeof(mensagens)-sizeof(long),0)==-1){
               printf("ERRO a enviar mensagem.\n");
            }     
        }
    }
}

void * decrementa_fuel_eta(void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);
    while(1){
        pthread_mutex_lock(&mutex_array_atr);
        for(int i=0; i < gs_configuracoes.qnt_max_chegadas; i++){
            array_voos_chegada[i].eta--;
            array_voos_chegada[i].fuel--;
            if(array_voos_chegada[i].fuel == 0){
                //remover da fila de espera
                pthread_mutex_lock(&mutex_fila_chegadas);
                remove_por_id(i);
                pthread_mutex_unlock(&mutex_fila_chegadas);

                sem_post(enviar_sinal);     //enviar sinal
                sem_wait(sinal_enviado);    //esperar pela confirmacao
            }
            if(i == 0){
                printf("fuel: %d\n", array_voos_chegada[i].fuel);
            }
        }
        pthread_mutex_unlock(&mutex_array_atr);
        usleep(gs_configuracoes.unidade_tempo * 1000);
    }
}

void * enviar_sinal_threads(void*t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);
    while(1){
        sem_wait(enviar_sinal);         //Esperar para receber o sinal

        pthread_cond_broadcast(&check_atr);

        sem_post(sinal_enviado);        //devolver o sinal
    }
}

void * wait_lists (void * t){
    sigset_t block_signals;
    sigfillset(&block_signals);
    pthread_sigmask (SIG_BLOCK, &block_signals, NULL);

    sem_wait(terminar_server);     //esperar pela indicacao que a torre de controlo vai terminar
    //esperar para todas os voos aterrarem e partirem

    pthread_mutex_lock(&mutex_fila_chegadas);
    pthread_mutex_lock(&mutex_fila_partidas);
    while((fila_espera_chegadas->next)!=NULL && (fila_espera_partidas!=NULL)){
        pthread_mutex_unlock(&mutex_fila_chegadas);
        pthread_mutex_unlock(&mutex_fila_partidas);
        usleep(gs_configuracoes.unidade_tempo * 1000);
        pthread_mutex_lock(&mutex_fila_chegadas);
        pthread_mutex_lock(&mutex_fila_partidas);
    }

    pthread_mutex_unlock(&mutex_fila_chegadas);
    pthread_mutex_unlock(&mutex_fila_partidas);

    //remover header node da fila de espera
    free(fila_espera_chegadas);

    //terminar threads do processo torre de controlo
    pthread_cancel(thread_msq);
    pthread_cancel(thread_fuel);

    sem_post(server_terminado);     //enviar sinal a indicar que a torre de controlo acabou os seus recursos
   
    pthread_exit(NULL);
}

void * receber_comandos(void * t){
    while(1){
        char * command;
        read(fd_pipe,comando,MAX_SIZE_COMANDO);
        command = strtok(comando, "\n");
        sem_wait(sem_log);
        sprintf(mensagem, "COMMAND IGNORED => %s", command);
        write_log(mensagem);
        sem_post(sem_log);
    }  
}

void termination_handler(int signo){
    pthread_t thread_comandos;
    pthread_create(&thread_comandos, NULL, receber_comandos, NULL);
    running = 1;
    printf("\nServer shutting down, waiting for all flights...\n");

    //esperar pelas threads criadoras de voos
    while(thread_list_prt!=NULL && (thread_list_atr!=NULL)){
        usleep(gs_configuracoes.unidade_tempo * 1000);
    }

    sem_post(terminar_server);          //enviar sinal para terminar a torre de controlo
    //printf("sem - %p\n", terminar_server);
    sem_wait(server_terminado);         //esperar pela resposta

    //terminar processo torre de controlo
    kill(pid, SIGKILL);

    //terminar as threads do precesso Gestor de Simulaçao
    pthread_cond_signal(&is_atr_list_empty);
    pthread_cond_signal(&is_prt_list_empty);
    pthread_cancel(thread_sinais);
    
    //remove mutexes
    pthread_mutex_destroy(&mutex_list_atr);
    pthread_mutex_destroy(&mutex_list_prt);
    pthread_mutex_destroy(&mutex_array_atr);
    pthread_mutex_destroy(&mutex_array_prt);
    pthread_mutex_destroy(&mutex_28L);
    pthread_mutex_destroy(&mutex_28R);
    pthread_mutex_destroy(&mutex_01L);
    pthread_mutex_destroy(&mutex_01R);
    pthread_mutex_destroy(&mutex_fila_chegadas);
    pthread_mutex_destroy(&mutex_fila_partidas);
    
    //remove semaforos
    sem_unlink(LOG_SEMAPHORE);
    sem_close(sem_log);
    sem_unlink(STATS_SEMAPHORE);
    sem_close(sem_estatisticas);
    sem_unlink(SEND_SIGNAL);
    sem_close(enviar_sinal);
    sem_unlink(SIGNAL_SENT);
    sem_close(sinal_enviado);
    sem_unlink(SERVER_TERMINATED);
    sem_close(server_terminado);

    //remove shared memory
    if(munmap(array_voos_chegada, gs_configuracoes.qnt_max_chegadas * sizeof(voo_chegada)) == -1){
        perror(NULL);
        printf("Error unmapping memory: %d\n", errno);
        exit(1);
    }
        
    close(shmid_arr);

    if(shm_unlink(SHM_ARR) == -1){
        printf("Error unlinking memory\n");
        exit(1);
    }

   
    if(munmap(estatisticas, sizeof(estatisticas_sistema)) == -1){
        perror(NULL);
        printf("Error unmapping memory: %d\n", errno);
        exit(1);
    }
        
    close(shmid_stats);

    if(shm_unlink(SHM_STATS) == -1){
        printf("Error unlinking memory\n");
        exit(1);
    }

    if(munmap(array_voos_partida, gs_configuracoes.qnt_max_partidas * sizeof(voo_partida)) == -1){
        perror(NULL);
        printf("Error unmapping memory: %d\n", errno);
        exit(1);
    }
        
    close(shmid_dep);

    if(shm_unlink(SHM_DEP) == -1){
        printf("Error unlinking memory\n");
        exit(1);
    }
    
    //remove message queue
    if (msgctl(msg_q_id, IPC_RMID, NULL) == -1 ){
        printf("Error removing message queue\n");
        exit(1);
    }
    
    printf("1\n");
    printf("thread %ld\n", thread_criadora_partidas);
    if (thread_criadora_partidas != 0)
        pthread_join(thread_criadora_chegadas, NULL);
    //remove conditional variables
    printf("---");
    printf("%d\n", pthread_join(thread_criadora_partidas, NULL));
    printf("2\n");
    //pthread_cond_destroy(&is_atr_list_empty);
    //pthread_cond_destroy(&is_prt_list_empty);
    //pthread_cond_destroy(&check_atr);
    //pthread_cond_destroy(&check_prt);
    //pthread_cond_destroy(&nmr_aterragens);
    printf("3\n");

    //remove pipe
    unlink(PIPE_NAME);
    close(fd_pipe);
 
    pthread_cancel(thread_comandos);

    printf("Server shutting down now\n");
    exit(0);
}

void ordena_ETA(){
    int troca=0;
    voos_chegada temp;
    voos_chegada temp1 = NULL;
    temp= fila_espera_chegadas;

    while(troca){
        while (temp->next != temp1){
            if(temp->eta > temp->next->eta){
                swap(temp, temp->next);
                troca=1;
            }
            temp=temp->next;
        }
        temp1=temp;
    }
}

void swap(voos_chegada x, voos_chegada y){
    int aux= x->eta;
    x->eta= y->eta;
    y->eta= aux;
}

void * holding(void *t){
    int contador=0, max=0;
    int holding;
    while (1){
        pthread_mutex_lock(&mutex_fila_chegadas);
        if (fila_espera_chegadas->id_slot_shm >5){
            voos_chegada atual= fila_espera_chegadas;
            while ((atual->next!=NULL)){
                contador++;
                if(contador>5){
                    holding=(rand() % (gs_configuracoes.dur_max-gs_configuracoes.dur_min+1)) + gs_configuracoes.dur_min;
                    atual->eta+=holding;
                    pthread_mutex_lock(&mutex_array_atr);
                    array_voos_chegada[atual->id_slot_shm].eta+=holding;
                    //verificar fuel e enviar sinal
                    pthread_mutex_unlock(&mutex_array_atr);
                    sem_wait(sem_log);
                    sprintf(mensagem, "%s HOLDING %d", array_voos_chegada[atual->id_slot_shm].flight_code, holding);
                    write_log(mensagem);
                    sem_post(sem_log);
                    if(holding>max){
                        max=holding;
                    }
                }
                atual= atual->next; 
            }
            ordena_ETA();
            usleep(max*1000);
        }
        pthread_mutex_unlock(&mutex_fila_chegadas);
    }
}

void sinal_estatisticas() {

    printf("Número total de voos criados: %d\n", estatisticas->n_voos_criados);
    printf("Número total de voos arerrados: %d\n", estatisticas->n_voos_aterrados);
    printf("Número total de voos descolados: %d\n", estatisticas->n_voos_descolados);
    printf("Número total de voos redirecionados: %d\n", estatisticas->n_voos_redirecionados);
    printf("Número total de voos rejeitados pela Torre de Controlo: %d\n", estatisticas->n_voos_rejeitados);
    printf("Tempo médio de espera para aterrar: %f\n", estatisticas->tempo_medio_aterrar);
    printf("Tempo médio de espera para descolar: %f\n", estatisticas->tempo_medio_descolar);
    printf("Número médio de manobras de holding por voo de aterragem: %f\n", estatisticas->n_medio_holdings_aterragem);
    printf("Número médio de manobras de holding por voo urgente: %f\n", estatisticas->n_medio_holdings_aterragem);

  }