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

void init(int argc, char **argv){
    startTime=malloc(sizeof(struct timeval));
    gettimeofday(startTime,0);
    load_args(argc,argv);
}

void *utilizador();
int i=0;
int fifo;
int main(int argc, char **argv)
{
    init(argc,argv);

    mkfifo(arguments.fifoname,0600); //criar a fifo publica
    fifo=open(arguments.fifoname,O_NONBLOCK); //abre a fifo pública

    //loop principal
    double t=0;
    while ((t=timeSinceStartTime())/1000<arguments.secs) {
        msleep(1);
        pthread_t t;
        pthread_create(&t,NULL,utilizador,NULL);
    }

    close(fifo);
    if((unlink(arguments.fifoname)))
        printf("%s\n",strerror(errno));
    return 0;
}

void *utilizador(){

    //gera tempo aleatório
    unsigned seed=time(NULL);
    int dur=rand_r(&seed)%49+1;

    //incrementa o i, apenas um pode aceder de cada vez
    pthread_mutex_lock(&add_i);
    i++;
    pthread_mutex_unlock(&add_i);

    //cria a struct request que vai ser enviada para o fifo
    request tmp={i,getpid(),gettid(),dur,-1};

    //cria o fifo privado
    int private_fifo;
    char fifo_name[24];
    sprintf(fifo_name,"%i.%i",getpid(),gettid());
    mkfifo(fifo_name,0600); //criar a fifo publica
    private_fifo=open(fifo_name,O_RDONLY);

    //bloqueia o acesso ao fifo, apenas um thread de cada vez pode escrever
    pthread_mutex_lock(&write_fifo);
    write(fifo,&tmp,sizeof(tmp));
    pthread_mutex_unlock(&write_fifo);

    //lê do fifo_privado
    read(private_fifo,&tmp,sizeof(tmp));
    
    //printf("ola\n");
    close(private_fifo);
    if(unlink(fifo_name))
        printf("%s\n",strerror(errno));
    return NULL;
}



