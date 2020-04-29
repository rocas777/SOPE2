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

#define ATTEMPTS 1000000

struct
{
    long unsigned secs;
    char *fifoname;
} typedef args;

//por alguma razão o gettid não estava definido
pid_t gettid()
{
    return syscall(SYS_gettid);
}

void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;

    // Storing start time
    clock_t start_time = clock();

    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds)
        ;
}

pthread_mutex_t place_mod = PTHREAD_MUTEX_INITIALIZER;

struct timeval *startTime;
args arguments;
pthread_t *queue;
int max = 100;
int maxFreeSpots = MAX_THREAD;
pthread_t *spots;

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

    do
    {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

double timeSinceStartTime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void load_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        argv++;
        //se for o argumento tempo
        if (strncmp(*argv, "-t", 2) == 0)
        {
            i++;
            argv++;
            arguments.secs = atoi(*argv);
        }
        else
        { // se for o argumento fifoname
            arguments.fifoname = *argv;
        }
    }
}

void init(int argc, char **argv)
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);
    load_args(argc, argv);
    queue = malloc(sizeof(pthread_t) * max);
    spots = malloc(sizeof(pthread_t) * maxFreeSpots + 1);

    if (mkfifo(arguments.fifoname, 0600) < 0)
    {
        //Cria a fifo publica e analiza se é válida.
        perror("ERROR setting up FIFO on main() of Q.c ");
        //exit(errno);
    }
}

long int place=0;

void *processRequest(void *input){

    //msleep(10);
    request local=*(request*)input;
    //printf("Bef - (Q.c) % i %i %i %f %i\n", local.i, local.pid, local.tid, local.dur, local.pl);
    char fifoLoad[599];
    char tmp_[599];
    sprintf(fifoLoad, "%i.%i", local.pid, local.tid);
    sprintf(tmp_,"/tmp/%s",fifoLoad);
    sprintf(fifoLoad,"%s",tmp_);
    int tmp;
    if((tmp = open(fifoLoad, O_WRONLY))==-1)
        perror("erro\n");
    fflush(stderr);
    pthread_mutex_lock(&place_mod);
    if(local.pl)
        local.pl=++place;
    else {
        local.pl=-1;
    }
    pthread_mutex_unlock(&place_mod);
    while(!write(tmp,&local,sizeof(request))){}
    printf("OK -% i %i %i %f %i\n",  local.i, local.pid, local.tid, local.dur, local.pl);
    //printf("Done %i\n\n",local.i);
    fflush(stdout);

    if(local.pl!=-1)
        msleep(local.dur);

    close(tmp);
    return NULL;
}

int i = 0;
int fifo;

int main(int argc, char **argv)
{
    init(argc, argv);
    request input;
    //printf("Tentou\n");
    fifo = open(arguments.fifoname, O_RDONLY); //abre a fifo pública
    //printf("Opened\n");
    double ti;
    //request u = {0,0,0,0,0};
    while (((ti = timeSinceStartTime()) / 1000) < arguments.secs)
    {
        if (read(fifo, &input, sizeof(input)))
        {
            //printf("Read %i\n",input.i);
            fflush(stdout);
            pthread_t t;
            pthread_create(&t,NULL,processRequest,&input);
        }
    }
    //printf("Closed\n");
    
    //while (((ti = timeSinceStartTime()) / 1000))
    //{
    int u;
    if (unlink(arguments.fifoname))
        printf("Erro 2 (com '%i'): %s\n", fifo, strerror(errno));
    msleep(10);
    while((u=read(fifo, &input, sizeof(input))))
    {
        //printf("Read %i\n",input.i);
        fflush(stdout);
        pthread_t t;
        input.pl=0;
        pthread_create(&t,NULL,processRequest,&input);
    }
    //printf("%i\n\n",u);
    //}
    

    free(startTime);
    free(queue);
    close(fifo);
    return 0;
}

