#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "types.h"
#include <sys/syscall.h>
struct{
    long unsigned secs;
    char *fifoname;
}typedef args ;

//por alguma razão o gettid não estava definido
pid_t gettid(){
    return syscall(SYS_gettid);
}

pthread_mutex_t add_i = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_fifo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_queue = PTHREAD_MUTEX_INITIALIZER;
struct timeval *startTime;
args arguments;

int msleep(long tms)
{
    struct timespec ts;
    int ret;

    if (tms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

double timeSinceStartTime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double) (instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void load_args(int argc, char **argv){
    for(int i=1;i<argc;i++){
        argv++;
        //se for o argumento tempo
        if(strncmp(*argv,"-t",2)==0){
            i++;
            argv++;
            arguments.secs = atoi(*argv);
        }
        else { // se for o argumento fifoname
            arguments.fifoname=*argv;
        }
    }
}

pthread_t *queue;
int max=100;
void init(int argc, char **argv){
    startTime=malloc(sizeof(struct timeval));
    gettimeofday(startTime,0);
    load_args(argc,argv);
    queue=malloc(sizeof(pthread_t)*max);
}
int arr_size=0;

void *utilizador();
int i=0;
FILE *fifo;
int main(int argc, char **argv)
{
    init(argc,argv);

    mkfifo(arguments.fifoname,0600); //criar a fifo publica
    fifo=fopen(arguments.fifoname,"w"); //abre a fifo pública

    //loop principal
    printf("Started\n");
    double t=0;
    int threads=0;
    while ((t=timeSinceStartTime())/1000<arguments.secs) {
        msleep(1);
        pthread_t t;
	int err;
        if((err=pthread_create(&t,NULL,utilizador,NULL)))
		printf("%i\n",err);
	threads++;    //free threads

	//limitar o num de threads por causa dos ficheiros abertos
	if(threads>1020){
    		pthread_mutex_lock(&add_queue);
   		while(arr_size){
			while(arr_size){
				pthread_join(queue[--arr_size],NULL);
				threads--;
    			}
    		}
    		pthread_mutex_unlock(&add_queue);
	}
    }
    printf("Thread creation Ended\n");

    //free threads
    pthread_mutex_lock(&add_queue);
    while(arr_size){
	pthread_mutex_lock(&add_queue);
	while(arr_size){
		pthread_join(queue[--arr_size],NULL);
    	}
    	pthread_mutex_unlock(&add_queue);
    }
    pthread_mutex_unlock(&add_queue);

    printf("Program Ended\n");
    fclose(fifo);
    free(startTime);
    free(queue);
    if((unlink(arguments.fifoname)))
        printf("%s\n",strerror(errno));
    return 0;
}

void *utilizador(){
    pthread_mutex_lock(&add_queue);
    queue[arr_size++]=pthread_self();
    if(arr_size>=max){
		queue=realloc(queue,max*10*sizeof(pthread_t));
		max*=10;    
		printf("queue resized: %i %lu\n",max,sizeof(pthread_t));
    }
    pthread_mutex_unlock(&add_queue);

    //int u=0;
    //gera tempo aleatório
    unsigned seed=time(NULL);
    int dur=rand_r(&seed)%49+1;

    //incrementa o i, apenas um pode aceder de cada vez
    pthread_mutex_lock(&add_i);
    i++;
    pthread_mutex_unlock(&add_i);
    //cria a struct request que vai ser enviada para o fifo
    request tmp={i,getpid(),gettid(),dur,-1};
    printf("% i %i %i %f %i\n",tmp.i,tmp.pid,tmp.tid,tmp.dur,tmp.pl); 

    //cria o fifo privado
    FILE *private_fifo;
    char fifo_name[50];
    sprintf(fifo_name,"%i.%i",getpid(),gettid());
    mkfifo(fifo_name,0600); //criar a fifo privada

    //bloqueia o acesso ao fifo, apenas um thread de cada vez pode escrever
    pthread_mutex_lock(&write_fifo);
    fwrite(&tmp,sizeof(request),1,fifo);
    pthread_mutex_unlock(&write_fifo);
    //printf("Escreveu\n");

    //abre o fifo privado
    private_fifo=fopen(fifo_name,"r");
    //printf("Abriu\n");



    

    //lê do fifo_privado
    fread(&tmp,sizeof(request),1,private_fifo);
    printf("Passou\n");
    
    
    if(fclose(private_fifo))
        printf("Erro 1:%s\n",strerror(errno));	
    sprintf(fifo_name,"%i.%i",getpid(),gettid());
    if(unlink(fifo_name))
        printf("Erro 2:%s\n",strerror(errno));
    printf("\n\n");
	
    //queue thread	
    return NULL;
}



