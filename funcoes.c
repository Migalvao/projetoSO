#include "header.h"

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
    time_t t_atual = (time(NULL) - t_inicial) * 1000;
    voo_partida * dados_partida = (voo_partida *)t;
    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut e quero partir no instante %d", dados_partida->flight_code, t_atual/gs_configuracoes.unidade_tempo, dados_partida->takeoff);
    write_log(mensagem);
    sem_post(sem_log);
    free(dados_partida);
    pthread_exit(NULL);
}

void * criar_partida(void * t){
    pthread_t thread_voo;
    time_t t_atual = (time(NULL) - t_inicial) * 1000;    
    //long init = (long)*((int *)t);   //em milissegundos
    voo_partida * dados_partida = (voo_partida *)t;
    long wait_time = (dados_partida->init * gs_configuracoes.unidade_tempo) - t_atual;        //em milissegundos
    printf("Vou esperar %ld segundos para gerar o voo\n", wait_time/1000);
    usleep(wait_time * 1000);   //microssegundos
    pthread_create(&thread_voo, NULL, partida,dados_partida);
    pthread_exit(NULL);
}

void * chegada(void * t){
    time_t t_atual = (time(NULL) - t_inicial) * 1000;
    voo_chegada * dados_chegada = (voo_chegada *)t;
    sem_wait(sem_log);
    sprintf(mensagem, "Sou o voo %s criado no instante %ld ut, eta = %d ut,fuel = %d", dados_chegada->flight_code, t_atual/gs_configuracoes.unidade_tempo, dados_chegada->eta, dados_chegada->fuel);
    write_log(mensagem);
    sem_post(sem_log);   
    free(dados_chegada);
    pthread_exit(NULL);
}

void * criar_chegada(void * t){
    pthread_t thread_voo;
    time_t t_atual = (time(NULL) - t_inicial) * 1000;    
    //long init = (long)*((int *)t);   //em milissegundos
    voo_chegada * dados_chegada = (voo_chegada *)t;
    long wait_time = (dados_chegada->init * gs_configuracoes.unidade_tempo) - t_atual;        //em milissegundos
    printf("Vou esperar %ld segundos para gerar o voo\n", wait_time/1000);
    usleep(wait_time * 1000);   //microssegundos
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
                partida.init=atoi(token);
                if ((partida.init * gs_configuracoes.unidade_tempo) <= t_atual)
                    return 1;
                //printf("%d\n",partida.init);
                token = strtok(NULL, delimitador);
                if (strcmp(token,"takeoff:")==0){
                    token = strtok(NULL, delimitador);
                    partida.takeoff=atoi(token);
                    if(partida.takeoff < partida.init)
                        return 1;
                    //printf("%d\n",partida.takeoff);
                    token = strtok(NULL, delimitador);
                    if(token == NULL){
                        dados_partida = (voo_partida *)malloc(sizeof(voo_partida));
                        strcpy(dados_partida->flight_code, partida.flight_code);
                        dados_partida->init = partida.init;
                        dados_partida->takeoff = partida.takeoff;
                        pthread_create(&thread_intermedia, NULL, criar_partida, dados_partida);
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
                chegada.init=atoi(token);
                if ((chegada.init * gs_configuracoes.unidade_tempo) <= t_atual)
                    return 1;
                //printf("%d\n", chegada.init);
                token= strtok(NULL,delimitador);
                if (strcmp(token,"eta:")==0){
                    token=strtok(NULL, delimitador);
                    chegada.eta=atoi(token);
                    //printf("%d\n", chegada.eta);
                    token= strtok(NULL,delimitador);
                    if (strcmp(token, "fuel:")==0){
                        token=strtok(NULL, delimitador);
                        chegada.fuel=atoi(token);
                        if(chegada.fuel < chegada.eta)
                            return 1;
                        //printf("%d\n", chegada.fuel);
                        token= strtok(NULL,delimitador);
                        if(token == NULL){
                            dados_chegada = (voo_chegada *)malloc(sizeof(voo_chegada));
                            strcpy(dados_chegada->flight_code, chegada.flight_code);
                            dados_chegada->init = chegada.init;
                            dados_chegada->eta = chegada.eta;
                            dados_chegada->fuel = chegada.fuel;
                            pthread_create(&thread_intermedia, NULL, criar_chegada, dados_chegada);
                            return 0;
                        }
                    }
                }
            }
        }   
    return 1;
    }
}
