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

#include <sys/stat.h> // stat
#include <stdbool.h>  // bool type

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

bool file_exists(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

pthread_mutex_t add_i = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_fifo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t t_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tvar = PTHREAD_COND_INITIALIZER;

pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;

struct timeval *startTime;
args arguments;

long u = 0;
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

void printIWANT(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; IWANT\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printIAMIN(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; IAMIN\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printCLOSD(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; CLOSD\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printFAILD(request *req)
{
    printf("%f ; %i ; %i ; %i ; %f ; %i ; FAILD\n", timeSinceStartTime(), req->i, req->pid, req->tid, req->dur, req->pl);
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

int max = 100;
void init(int argc, char **argv)
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);
    load_args(argc, argv);
}
int arr_size = 0;

void *utilizador();
int i = 0;
int fifo;

int out = 1;

int threads = 0;

int gid = 0;

int main(int argc, char **argv)
{
    init(argc, argv);

    fifo = open(arguments.fifoname, O_WRONLY);
    //abre a fifo pública

    //printf("FIFO at '%s' created and opened with success!\n", arguments.fifoname);

    //loop principal
    fflush(stdout);
    double t = 0;
    while ((t = timeSinceStartTime()) / 1000 <= arguments.secs && out)
    {
        msleep(1);
        pthread_t t;
        int err;
        pthread_mutex_lock(&t_queue);
        threads++;
        if (threads > 5000)
            pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);

        while ((err = pthread_create(&t, NULL, utilizador, NULL)))
        {
            printf("Erro 1: %i\n", threads);
            msleep(1);
        }
        if (i > u)
            u = i;
    }

    //free threads

    msleep(10);
    printf("Erro 2: %i\n", threads);

    printf("Program Ended %li\n", u);

    close(fifo);
    free(startTime);

    exit(0);
}

void *utilizador()
{
    //gera tempo aleatório
    unsigned seed = time(NULL) + i;
    int dur = rand_r(&seed) % 49 + 1;

    //incrementa o i, apenas um pode aceder de cada vez
    pthread_mutex_lock(&add_i);
    i++;
    pthread_mutex_unlock(&add_i);

    //cria a struct request que vai ser enviada para o fifo
    request tmp = {i, getpid(), gettid(), dur, -1};

    printIWANT(&tmp);
    fflush(stdout);

    //cria o fifo privado
    int private_fifo;
    char fifo_name[599];
    sprintf(fifo_name, "/tmp/%i.%i", tmp.pid, tmp.tid);

    if (mkfifo(fifo_name, 0600) < 0)
    {
        //Cria a fifo privada e analiza se é válida.
        perror("ERROR setting up private FIFO on utilizador() of U.c ");
        exit(errno);
    }

    if (!file_exists(arguments.fifoname) || write(fifo, &tmp, sizeof(request)) == -1)
    {
        printFAILD(&tmp);

        if (unlink(fifo_name))
            printf("Erro 3 (com '%s'): %s\n", fifo_name, strerror(errno));

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
        out = 0;
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&add_queue);
    if (arr_size >= 1000)
        pthread_cond_wait(&cvar, &add_queue);
    arr_size++;
    pthread_mutex_unlock(&add_queue);
    if ((private_fifo = open(fifo_name, O_RDONLY)) == -1)
    {
        printFAILD(&tmp);
        perror("erro\n");
    }
    fflush(stdout);

    // read(private_fifo, &tmp, sizeof(request));

    if (!file_exists(fifo_name) || read(private_fifo, &tmp, sizeof(request)) == -1)
    {
        printFAILD(&tmp);
    }
    else
    {
        if (tmp.pl != -1)
            printIAMIN(&tmp);
        else
            printCLOSD(&tmp);
    }
    fflush(stdout);

    if (unlink(fifo_name))
        printf("Erro 3 (com '%s'): %s\n", fifo_name, strerror(errno));

    if (close(private_fifo))
        printf("Erro 4:%s\n", strerror(errno));

    //queue thread

    pthread_mutex_lock(&add_queue);
    arr_size--;
    if (arr_size < 1000)
        pthread_cond_signal(&cvar);
    pthread_mutex_unlock(&add_queue);
    //printf("out - (U.c) % i\n\n", tmp.i);

    pthread_mutex_lock(&t_queue);
    threads--;
    if (threads < 5000)
        pthread_cond_signal(&tvar);
    pthread_mutex_unlock(&t_queue);
    pthread_exit(NULL);
}
