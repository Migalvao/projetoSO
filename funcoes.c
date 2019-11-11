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

void * voo(){
    time_t t_atual = (time(NULL) - t_inicial) * 1000;
    sem_wait(sem_log);
    sprintf(mensagem, "Voo criado no instante %ld ut", t_atual/gs_configuracoes.unidade_tempo);
    write_log(mensagem);
    sem_post(sem_log);
    pthread_exit(NULL);
}

void * criar_thread(void * t){
    pthread_t thread_voo;
    time_t t_atual = (time(NULL) - t_inicial) * 1000;    
    long init = (long)*((int *)t);   //em milissegundos
    long wait_time = (init * gs_configuracoes.unidade_tempo) - t_atual;        //em milissegundos
    printf("Vou esperar %ld segundos para gerar o voo\n", wait_time/1000);
    usleep(wait_time * 1000);   //microssegundos
    pthread_create(&thread_voo, NULL, voo,NULL);
    pthread_exit(NULL);
}

int validacao_pipe(char * comando, int * init){
    char delimitador[]= " ";
    char *token;
    char copia_comando[MAX_SIZE_COMANDO];
    time_t t_atual = (time(NULL) - t_inicial) * 1000;  
    voo_chegada chegada;
    voo_partida partida;
    
    strcpy(copia_comando, comando);
    token = strtok(copia_comando, delimitador);
    /*
    Vamos ter de criar uma versao diferente desta funçao. Para validar mesmo so o comando so
    precisamos mesmo de partir as coisas(sem as guardar) e ficar com o init.
    Depois, para guardar as informaçoes ai ja vamos precisar de guardar as coisas numa estrutura
    Vamos fazer isso dando o ponteiŕo da estrutura para guardar
    */


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
                (*init)=partida.init;
                //printf("%d\n",partida.init);
                token = strtok(NULL, delimitador);
                if (strcmp(token,"takeoff:")==0){
                    token = strtok(NULL, delimitador);
                    partida.takeoff=atoi(token);
                    if(partida.takeoff < partida.init)
                        return 1;
                    //printf("%d\n",partida.takeoff);
                    token = strtok(NULL, delimitador);
                    return 0;
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
                (*init)=atoi(token);
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
                        return 0;
                    
                    }
                }
            }
        }   
    return 1;
    }
}
