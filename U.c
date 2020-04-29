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
    fflush(stdout);
}

void printIWANT(request *req)
{
    printRequest(req);
    printf("IWANT\n");
    fflush(stdout);
}

void printIAMIN(request *req)
{
    printRequest(req);
    printf("IAMIN\n");
    fflush(stdout);
}

void printCLOSD(request *req)
{
    printRequest(req);
    printf("CLOSD\n");
    fflush(stdout);
}

void printFAILD(request *req)
{
    printRequest(req);
    printf("FAILD\n");
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

pthread_t *queue;
int max = 100;
void init(int argc, char **argv)
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);
    load_args(argc, argv);
    queue = malloc(sizeof(pthread_t) * max);
}
int arr_size = 0;

void *utilizador();
int i = 0;
int fifo;

int out = 1;

int main(int argc, char **argv)
{
    init(argc, argv);

    fifo = open(arguments.fifoname, O_WRONLY);
    //abre a fifo pública

    //printf("FIFO at '%s' created and opened with success!\n", arguments.fifoname);

    //loop principal
    //printf("Started\n");
    fflush(stdout);
    double t = 0;
    int threads = 0;
    while ((t = timeSinceStartTime()) / 1000 < arguments.secs && out)
    {
        msleep(1);
        pthread_t t;
        int err;
        if ((err = pthread_create(&t, NULL, utilizador, NULL)))
            printf("%i\n", err);
        threads++; //free threads

        //limitar o num de threads por causa dos ficheiros abertos
        if (threads > 10)
        {
            pthread_mutex_lock(&add_queue);
            while (arr_size)
            {
                pthread_join(queue[--arr_size], NULL);
                threads--;
            }
            pthread_mutex_unlock(&add_queue);
        }
    }
    //printf("Thread creation Ended\n");

    //free threads
    //msleep(10);
    while (arr_size)
    {
        pthread_mutex_lock(&add_queue);
        if (arr_size)
        {
            pthread_join(queue[--arr_size], NULL);
            threads--;
        }
        pthread_mutex_unlock(&add_queue);
    }
    printf("%i\n", threads);

    //printf("Program Ended\n");

    close(fifo);
    free(startTime);
    free(queue);
    exit(0);
}

void *utilizador()
{
    //int u=0;
    //gera tempo aleatório
    unsigned seed = time(NULL) + i;
    int dur = rand_r(&seed) % 49 + 1;

    //incrementa o i, apenas um pode aceder de cada vez
    pthread_mutex_lock(&add_i);
    i++;
    //printf("in - (U.c) % i\n", i);
    pthread_mutex_unlock(&add_i);

    //cria a struct request que vai ser enviada para o fifo
    request tmp = {i, getpid(), gettid(), dur, -1};
    printIWANT(&tmp);

    //cria o fifo privado
    int private_fifo;
    char fifo_name[599];
    char tmp_[599];
    sprintf(fifo_name, "%i.%i", tmp.pid, tmp.tid);
    sprintf(tmp_, "/tmp/%s", fifo_name);
    sprintf(fifo_name, "%s", tmp_);
    //printf("MkFifo - (U.c) % i\n", i);
    if (mkfifo(fifo_name, 0600) < 0)
    {
        //Cria a fifo privada e analiza se é válida.
        perror("ERROR setting up private FIFO on utilizador() of U.c ");
        exit(errno);
    }

    //pthread_mutex_lock(&write_fifo);
    //printf("Be -% i %i %i %f %i\n", tmp.i, tmp.pid, tmp.tid, tmp.dur, tmp.pl);
    if (!file_exists(arguments.fifoname) || write(fifo, &tmp, sizeof(request)) == -1)
    {
        //printf("erro 2 %i\n",tmp.i);
        pthread_mutex_lock(&add_queue);
        printf("ERRO\n");
        fflush(stdout);
        out = 0;
        queue[arr_size++] = pthread_self();
        if (arr_size >= max)
        {
            queue = realloc(queue, max * 10 * sizeof(pthread_t));
            max *= 10;
            //printf("queue resized: %i %lu\n", max, sizeof(pthread_t));
        }
        pthread_mutex_unlock(&add_queue);
        pthread_exit(NULL);
    }
    //printf("Wrote\n");
    fflush(stdout);

    //pthread_mutex_unlock(&write_fifo);
    //printf("Escreveu\n");

    //abre o fifo privado
    //printf("Abriu\n");

    //lê do fifo_privado
    //msleep(10);
    if ((private_fifo = open(fifo_name, O_RDONLY)) == -1)
    {
        perror("erro\n");
        printFAILD(&tmp);
    }
    fflush(stdout);
    read(private_fifo, &tmp, sizeof(request));
    // printf("OK -% i %i %i %f %i\n", tmp.i, tmp.pid, tmp.tid, tmp.dur, tmp.pl);
    if (tmp.pl != -1)
        printIAMIN(&tmp);
    else
        printCLOSD(&tmp);

    fflush(stdout);

    if (unlink(fifo_name))
        printf("Erro 2 (com '%s'): %s\n", fifo_name, strerror(errno));

    if (close(private_fifo))
        printf("Erro 1:%s\n", strerror(errno));

    //queue thread

    pthread_mutex_lock(&add_queue);
    queue[arr_size++] = pthread_self();
    if (arr_size >= max)
    {
        queue = realloc(queue, max * 10 * sizeof(pthread_t));
        max *= 10;
        //printf("queue resized: %i %lu\n", max, sizeof(pthread_t));
    }
    pthread_mutex_unlock(&add_queue);
    //printf("out - (U.c) % i\n\n", tmp.i);

    pthread_exit(NULL);
}
