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
pthread_mutex_t access_input = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_queue = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;

pthread_mutex_t t_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tvar = PTHREAD_COND_INITIALIZER;

struct timeval *startTime;
args arguments;
pthread_t *queue;
int max = 100;
int maxFreeSpots = MAX_THREAD;
pthread_t *spots;
long int arr_size=0;
int threads=0;
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

void printRequest(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; ", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    // fflush(stdout);
}

void printRECVD(request *req)
{
    printRequest(req);
    printf("RECVD\n");
    fflush(stdout);
}

void printENTER(request *req)
{
    printRequest(req);
    printf("ENTER\n");
    fflush(stdout);
}

void printTIMUP(request *req)
{
    printRequest(req);
    printf("TIMUP\n");
    fflush(stdout);
}

void print2LATE(request *req)
{
    printRequest(req);
    printf("2LATE\n");
    fflush(stdout);
}

void printGAVUP(request *req)
{
    printRequest(req);
    printf("GAVUP\n");
    fflush(stdout);
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

long int place = 0;

void *processRequest(void *input)
{

    //msleep(10);
    //pthread_mutex_lock(&access_input);
    request local;
    memcpy(&local, input, sizeof(request));
    free(input);
    //pthread_mutex_unlock(&access_input);
    //printf("Be -% i %i %i %f %i\n", local.i, local.pid, local.tid, local.dur, local.pl);
    fflush(stdout);
    char fifoLoad[599];
    char tmp_[599];
    sprintf(fifoLoad, "%i.%i", local.pid, local.tid);
    sprintf(tmp_, "/tmp/%s", fifoLoad);
    sprintf(fifoLoad, "%s", tmp_);
    int tmp;
    pthread_mutex_lock(&add_queue);
	arr_size++;
	if(arr_size>= 1000)
		pthread_cond_wait(&cvar, &add_queue);
    pthread_mutex_unlock(&add_queue);
    if((tmp = open(fifoLoad, O_WRONLY ))==-1){
        fprintf(stderr,"erro 1\n");
        fflush(stderr);
        //msleep(10);
    }
    pthread_mutex_lock(&place_mod);

    if (local.pl && ((local.dur + timeSinceStartTime()) / 1000) < arguments.secs)
    {
        local.pl = ++place;
    }
    else
    {
        local.pl = -1;
    }
    pthread_mutex_unlock(&place_mod);
    if (write(tmp, &local, sizeof(request)) == -1)
    {
        printGAVUP(&local);
        fprintf(stderr, "ERRO 2\n");
        fflush(stderr);
        pthread_exit(NULL);
    }

    printf("OK -% i %i %i %f %i\n",  local.i, local.pid, local.tid, local.dur, local.pl);
    //printf("Done %i\n\n",local.i);
    fflush(stdout);    

    close(tmp);

    pthread_mutex_lock(&add_queue);
	arr_size--;
	if(arr_size<1000)
		pthread_cond_signal(&cvar);
    pthread_mutex_unlock(&add_queue);

    if (local.pl != -1)
    {
        printENTER(&local);
        fflush(stdout);
        msleep(local.dur);
        printTIMUP(&local);
        fflush(stdout);
    }
    else
    {
        print2LATE(&local);
    }

    	pthread_mutex_lock(&t_queue);
		threads--;
		if(threads<5000)
			pthread_cond_signal(&tvar);
	pthread_mutex_unlock(&t_queue);
    pthread_exit(NULL);
}

int i = 0;
int fifo;

int main(int argc, char **argv)
{
    init(argc, argv);
    //printf("Tentou\n");
    fifo = open(arguments.fifoname, O_RDONLY | O_NONBLOCK); //abre a fifo pública
    //printf("Opened\n");
    double ti;
    request input;
    //request u = {0,0,0,0,0};
    while (((ti = timeSinceStartTime()) / 1000) < arguments.secs)
    {

        //pthread_mutex_lock(&access_input);
        if (read(fifo, &input, sizeof(input))>0)
        {
            //printf("Read %i\n",input.i);
            fflush(stdout);
            printRECVD(&input);

            request *tmp_r = malloc(sizeof(request));
            memcpy(tmp_r, &input, sizeof(request));
            pthread_t t;
	    pthread_mutex_lock(&t_queue);
		threads++;
		if(threads>5000)
			pthread_cond_wait(&tvar, &t_queue);
	    pthread_mutex_unlock(&t_queue);
            while (pthread_create(&t, NULL, processRequest, tmp_r))
            {
            }
        }
        //pthread_mutex_unlock(&access_input);
        //msleep(1);
    }

    //while (((ti = timeSinceStartTime()) / 1000))
    //{
    int u;
    if (unlink(arguments.fifoname))
        printf("Erro 2 (com '%i'): %s\n", fifo, strerror(errno));
    msleep(50);

    //pthread_mutex_lock(&access_input);
    while ((u = read(fifo, &input, sizeof(input))))
    {
        //printf("Read %i\n",input.i);
        fflush(stdout);
        printRECVD(&input);

        pthread_t t;

        input.pl=0;
	pthread_mutex_lock(&t_queue);
		threads++;
		if(threads>5000)
			pthread_cond_wait(&tvar, &t_queue);
	pthread_mutex_unlock(&t_queue);
        while (pthread_create(&t,NULL,processRequest,&input)) {}

    }
    //pthread_mutex_unlock(&access_input);
    //printf("%i\n\n",u);
    //}

    free(startTime);
    free(queue);
    close(fifo);
    exit(0);
}
