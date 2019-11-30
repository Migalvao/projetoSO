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

void * partida(void * t){
    time_t t_criacao = ((time(NULL) - t_inicial) * 1000)/gs_configuracoes.unidade_tempo;
    voo_partida * dados_partida = (voo_partida *)t;
    mensagens msg;

    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut,takeoff = %d", dados_partida->flight_code, t_criacao, dados_partida->takeoff);
    write_log(mensagem);
    sem_post(sem_log);

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

    sem_wait(sem_partidas);
    array_voos_partida[msg.id_slot_shm].takeoff = dados_partida->takeoff;
    array_voos_partida[msg.id_slot_shm].takeoff = dados_partida->init;
    strcpy(array_voos_partida[msg.id_slot_shm].flight_code, dados_partida->flight_code);
    sem_post(sem_partidas);

    printf("%s Guardei os meus dados de partida no slot %d\n", dados_partida->flight_code, msg.id_slot_shm);

    free(dados_partida);
    pthread_exit(NULL);
}

void * criar_partida(void * t){
    pthread_t thread_voo;
    voo_partida * dados_partida;
    time_t t_atual;        //em ut's
    while(1){

        pthread_mutex_lock(&mutex_list_prt);
        while(thread_list_prt == NULL)
            pthread_cond_wait(&is_prt_list_empty, &mutex_list_prt);
        t_atual = ((time(NULL) - t_inicial) * 1000) / gs_configuracoes.unidade_tempo;       //em ut's

        if(t_atual < thread_list_prt->voo.init){
            t_atual ++;
            usleep(gs_configuracoes.unidade_tempo * 1000);
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
    pthread_exit(NULL);
}

void * chegada(void * t){
    time_t t_criacao = ((time(NULL) - t_inicial) * 1000)/gs_configuracoes.unidade_tempo;    //em ut's
    time_t t_atual;
    voo_chegada * dados_chegada = (voo_chegada *)t;
    mensagens msg;

    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut, eta = %d ut,fuel = %d", dados_chegada->flight_code, t_criacao, dados_chegada->eta, dados_chegada->fuel);
    write_log(mensagem);
    sem_post(sem_log);   

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

    sem_wait(sem_partidas);
    array_voos_chegada[msg.id_slot_shm].eta = dados_chegada->eta;   //para tirar depois: deve ser a torre de controlo a colocar o eta
    array_voos_chegada[msg.id_slot_shm].fuel = dados_chegada->fuel;
    array_voos_chegada[msg.id_slot_shm].init = dados_chegada->init;
    strcpy(array_voos_chegada[msg.id_slot_shm].flight_code, dados_chegada->flight_code);
    sem_post(sem_partidas);

    printf("%s Guardei os meus dados de chegada no slot %d\n", dados_chegada->flight_code,msg.id_slot_shm);

    pthread_mutex_lock(&mutex_array_atr);
    while(array_voos_chegada[msg.id_slot_shm].fuel > 0 && array_voos_chegada[msg.id_slot_shm].eta > ((time(NULL) - t_inicial) * 1000)/gs_configuracoes.unidade_tempo)
        pthread_cond_wait(&is_prt_list_empty, &mutex_array_atr);    

    if(array_voos_chegada[msg.id_slot_shm].fuel == 0){
        array_voos_chegada[msg.id_slot_shm].fuel = -1;
        sem_wait(sem_log);
        sprintf(mensagem, "%s LEAVING TO OTHER AIRPORT => FUEL = 0", dados_chegada->flight_code);
        write_log(mensagem);
        sem_post(sem_log);
    }

    pthread_mutex_unlock(&mutex_array_atr);

    free(dados_chegada);
    pthread_exit(NULL);
}

void * criar_chegada(void * t){
    pthread_t thread_voo;
    voo_chegada * dados_chegada;
    time_t t_atual;        //em ut's
    while(1){

        pthread_mutex_lock(&mutex_list_atr);
        while(thread_list_atr == NULL)
            pthread_cond_wait(&is_atr_list_empty, &mutex_list_atr);
        t_atual = ((time(NULL) - t_inicial) * 1000) / gs_configuracoes.unidade_tempo;        //em ut's

        if(t_atual < thread_list_atr->voo.init){
            t_atual ++;
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
        }
        pthread_mutex_unlock(&mutex_list_atr);
    }
    pthread_create(&thread_voo, NULL, chegada,dados_chegada);
    pthread_exit(NULL);
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

voos_chegada adicionar_fila_chegadas(voos_chegada lista_chegadas, mensagens voo_cheg){
    voos_chegada nova_chegada= (voos_chegada)malloc(sizeof(node_chegadas));
    nova_chegada->next= NULL;
    nova_chegada->id_slot_shm = voo_cheg.id_slot_shm;
    nova_chegada->eta = voo_cheg.eta;

    if(lista_chegadas==NULL){
        lista_chegadas=nova_chegada;
        return lista_chegadas;
    }
    else{
        voos_chegada atual= lista_chegadas;
        while(atual->next!=NULL){
            if(atual->next->eta >= nova_chegada->eta){
                atual=atual->next;
            }else{
                nova_chegada->next=atual->next;
                atual->next=nova_chegada;
                return lista_chegadas;
            }
        }
        atual->next=nova_chegada;
        return lista_chegadas;
    }
}

voos_chegada adicionar_inicio(voos_chegada lista_prioritarios, mensagens voo_cheg){
    //adicionar voo marcado como urgente, ou seja, inserir no inicio da fila/lista
    voos_chegada prioritario= (voos_chegada)malloc(sizeof(node_chegadas));
    prioritario->next=NULL;
    prioritario->id_slot_shm = voo_cheg.id_slot_shm;
    prioritario->eta = voo_cheg.eta;

    if(lista_prioritarios==NULL){
        lista_prioritarios= prioritario;
        return lista_prioritarios;
    }
    else{
        prioritario->next=lista_prioritarios;
        lista_prioritarios=prioritario; 
    }
    return lista_prioritarios;
}

voos_partida remove_partida(voos_partida head){
    voos_partida aux = head;
    head = head->next;
    free(aux);
    return head;
}

voos_chegada remove_chegada(voos_chegada head){
    voos_chegada aux = head;
    head= head->next;
    free(aux);
    return head;
}

voos_chegada remove_por_id(voos_chegada head, int id){
    voos_chegada atual = head;
    voos_chegada anterior =  head;

    while (atual->next != NULL){
        if(id == atual->id_slot_shm){
            anterior->next = atual->next;
            free(atual);            
        }
    atual= atual->next;
    }
    return head;
}