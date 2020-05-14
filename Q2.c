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
#include <stdbool.h> // bool type
#include <time.h>

#define ATTEMPTS 1000000

struct
{
    long unsigned secs;
    int nplaces;
    int nthreads;
    char *fifoname;
} typedef args;

//por alguma razão o gettid não estava definido

bool file_exists(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

time_t start;

pthread_mutex_t place_mod = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pvar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t access_input = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_queue = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;

pthread_mutex_t t_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tvar = PTHREAD_COND_INITIALIZER;

struct timeval *startTime;
args arguments;
long int arr_size = 0;
int threads = 0;
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

double timeSinceStarttime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printRECVD(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; RECVD\n", time(NULL) - start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printENTER(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; ENTER\n", time(NULL) - start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printTIMUP(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; TIMUP\n", time(NULL) - start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void print2LATE(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; 2LATE\n", time(NULL) - start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printGAVUP(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; GAVUP\n", time(NULL) - start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

int load_args(int argc, char **argv)
{
    arguments.secs = 0;
    arguments.nplaces = __INT32_MAX__;
    arguments.nthreads = 5000;
    arguments.fifoname = "";
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
        else if (strncmp(*argv, "-l", 2) == 0)
        { // se for o argumento lotação do quarto de banho
            i++;
            argv++;
            arguments.nplaces = atoi(*argv);
        }
        else if (strncmp(*argv, "-n", 2) == 0)
        { // se for o argumento número máximo de threads a atender pedidos
            i++;
            argv++;
            arguments.nthreads = atoi(*argv);
        }
        else
        { // se for o argumento fifoname
            arguments.fifoname = *argv;
        }
    }
    if(arguments.nplaces < arguments.nthreads)
	arguments.nthreads = arguments.nplaces;
    if (arguments.secs == 0)
    {
        printf("ERRO nos Parametros!\n");
        return 1;
    }
    return 0;
}

long int place = 0;
int *places;

int init(int argc, char **argv)
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);
    if (load_args(argc, argv))
        return 1;

    places = malloc(sizeof(int) * arguments.nplaces);

    if (mkfifo(arguments.fifoname, 0600) < 0)
    {
        //Cria a fifo publica e analiza se é válida.
        perror("ERROR setting up FIFO on main() of Q.c ");
        return 1;
    }
    for(int i=0;i<arguments.nplaces;i++){
	places[i]=i;
    }
    place=arguments.nplaces-1;
    start = time(NULL);
    return 0;
}

void *processRequest(void *input)
{
    request local;
    memcpy(&local, input, sizeof(request));
    //free(input);

    char fifoLoad[599];
    sprintf(fifoLoad, "/tmp/%i.%i", local.pid, local.tid);
    int tmp;

    pthread_mutex_lock(&add_queue);
    arr_size++;
    if (arr_size >= 1000)
        pthread_cond_wait(&cvar, &add_queue);
    pthread_mutex_unlock(&add_queue);

    if ((tmp = open(fifoLoad, O_WRONLY)) == -1)
    {
        fprintf(stderr, "erro 1\n");
        fflush(stderr);
    }

    pthread_mutex_lock(&place_mod);
    if (local.dur != -1)
    {
        if (place >= 0)
        {
            local.pl = places[place--];
        }
        else
        {
            pthread_cond_wait(&pvar, &place_mod);
            local.pl = places[place--];
        }
    }
    pthread_mutex_unlock(&place_mod);

    if ((((timeSinceStarttime()) / 1000) > arguments.secs))
    {
        local.pl = -1;
        local.dur = -1;
    }

    if (!file_exists(fifoLoad) || write(tmp, &local, sizeof(request)) == -1)
    {
        printGAVUP(&local);
        fprintf(stderr, "ERRO 2\n");
        fflush(stderr);

        pthread_mutex_lock(&add_queue);
        arr_size--;
        if (arr_size < 1000)
            pthread_cond_signal(&cvar);
        pthread_mutex_unlock(&add_queue);

        pthread_mutex_lock(&place_mod);
    	places[++place] = local.pl;
        pthread_cond_signal(&pvar);
        pthread_mutex_unlock(&place_mod);

        pthread_mutex_lock(&t_queue);
        threads--;
        pthread_cond_signal(&tvar);
        pthread_mutex_unlock(&t_queue);
        pthread_exit(NULL);
    }

    close(tmp);

    pthread_mutex_lock(&add_queue);
    arr_size--;
    if (arr_size < 1000)
        pthread_cond_signal(&cvar);
    pthread_mutex_unlock(&add_queue);

    if (local.dur != -1)
    {
        printENTER(&local);
        msleep(local.dur);
        printTIMUP(&local);
    }
    else
    {
        print2LATE(&local);
    }

    pthread_mutex_lock(&place_mod);
    places[++place] = local.pl;
    pthread_cond_signal(&pvar);
    pthread_mutex_unlock(&place_mod);

    pthread_mutex_lock(&t_queue);
    threads--;
    pthread_cond_signal(&tvar);
    pthread_mutex_unlock(&t_queue);

    pthread_exit(NULL);
}

int i = 0;
int fifo;

int main(int argc, char **argv)
{
    if (init(argc, argv))
        exit(1);
    fifo = open(arguments.fifoname, O_RDONLY | O_NONBLOCK); //abre a fifo pública
    request input;

    while (((timeSinceStarttime()) / 1000) <= arguments.secs)
    {

        if (read(fifo, &input, sizeof(input)) > 0)
        {
            fflush(stdout);
            printRECVD(&input);

            request *tmp_r = malloc(sizeof(request));
            memcpy(tmp_r, &input, sizeof(request));
            pthread_t t;

            pthread_mutex_lock(&t_queue);
            threads++;
            if (threads > arguments.nthreads)
                pthread_cond_wait(&tvar, &t_queue);
            pthread_mutex_unlock(&t_queue);

            while (pthread_create(&t, NULL, processRequest, tmp_r))
            {
            }
        }
    }

    if (unlink(arguments.fifoname))
        fprintf(stderr, "Erro (com '%i'): %s\n", fifo, strerror(errno));

    while (threads > 0)
    {
        pthread_mutex_lock(&t_queue);
        pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);
    }

    while (read(fifo, &input, sizeof(input)) > 0)
    {
        fflush(stdout);
        printRECVD(&input);

        input.dur = -1;
        request *tmp_r = malloc(sizeof(request));
        memcpy(tmp_r, &input, sizeof(request));
        pthread_t t;
        pthread_mutex_lock(&t_queue);
        threads++;
        if (threads > arguments.nthreads)
            pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);
        while (pthread_create(&t, NULL, processRequest, tmp_r))
        {
        }
    }
    while (threads > 0)
    {
        pthread_mutex_lock(&t_queue);
        pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);
    }

    //free(places);
    //free(startTime);
    close(fifo);
    exit(0);
}
