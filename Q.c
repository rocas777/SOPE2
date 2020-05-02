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

#define ATTEMPTS 1000000

struct
{
    long unsigned secs;
    char *fifoname;
} typedef args;

//por alguma razão o gettid não estava definido

bool file_exists(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}


pthread_mutex_t place_mod = PTHREAD_MUTEX_INITIALIZER;
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

double timeSinceStartTime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printRECVD(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; RECVD\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printENTER(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; ENTER\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printTIMUP(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; TIMUP\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void print2LATE(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; 2LATE\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printGAVUP(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; GAVUP\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
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

    if (mkfifo(arguments.fifoname, 0600) < 0)
    {
        //Cria a fifo publica e analiza se é válida.
        perror("ERROR setting up FIFO on main() of Q.c ");
    }
}

long int place = 0;

void *processRequest(void *input)
{
    request local;
    memcpy(&local, input, sizeof(request));
    free(input);

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
        local.pl = ++place;
    }

    pthread_mutex_unlock(&place_mod);

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

        pthread_mutex_lock(&t_queue);
        threads--;
        if (threads < 5000)
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

    pthread_mutex_lock(&t_queue);
    threads--;
    if (threads < 5000)
        pthread_cond_signal(&tvar);
    pthread_mutex_unlock(&t_queue);
    pthread_exit(NULL);
}

int i = 0;
int fifo;

int main(int argc, char **argv)
{
    init(argc, argv);
    fifo = open(arguments.fifoname, O_RDONLY | O_NONBLOCK); //abre a fifo pública
    double ti;
    request input;

    while (((ti = timeSinceStartTime()) / 1000) <= arguments.secs)
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
            if (threads > 5000)
                pthread_cond_wait(&tvar, &t_queue);
            pthread_mutex_unlock(&t_queue);
            while (pthread_create(&t, NULL, processRequest, tmp_r))
            {
            }
        }
    }

    if (unlink(arguments.fifoname))
        fprintf(stderr,"Erro (com '%i'): %s\n", fifo, strerror(errno));

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
        if (threads > 5000)
            pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);
        while (pthread_create(&t, NULL, processRequest, tmp_r))
        {
        }
    }
    while (threads)
        msleep(1);

    free(startTime);
    close(fifo);
    exit(0);
}
