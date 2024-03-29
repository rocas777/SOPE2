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
#include <time.h>

#include <sys/stat.h> // stat
#include <stdbool.h>  // bool type

struct
{
    long unsigned secs;
    char *fifoname;
} typedef args;

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

time_t start;

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

double timeSinceStarttime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

// void printIWANT(request *req)
// {
//     printf("%f ; %i ; %i ; %i ; %f ; %i ; IWANT\n", timeSinceStarttime(), req->i, req->pid, req->tid, req->dur, req->pl);
//     fflush(stdout);
// }

// void printIAMIN(request *req)
// {
//     printf("%f ; %i ; %i ; %i ; %f ; %i ; IAMIN\n", timeSinceStarttime(), req->i, req->pid, req->tid, req->dur, req->pl);
//     fflush(stdout);
// }

// void printCLOSD(request *req)
// {
//     printf("%f ; %i ; %i ; %i ; %f ; %i ; CLOSD\n", timeSinceStarttime(), req->i, req->pid, req->tid, req->dur, req->pl);
//     fflush(stdout);
// }

// void printFAILD(request *req)
// {
//     printf("%f ; %i ; %i ; %i ; %f ; %i ; FAILD\n", timeSinceStarttime(), req->i, req->pid, req->tid, req->dur, req->pl);
//     fflush(stdout);
// }

void printIWANT(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; IWANT\n", time(NULL)-start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printIAMIN(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; IAMIN\n", time(NULL)-start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printCLOSD(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; CLOSD\n", time(NULL)-start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

void printFAILD(request *req)
{
    printf("%li ; %i ; %i ; %i ; %f ; %i ; FAILD\n", time(NULL)-start, req->i, req->pid, req->tid, req->dur, req->pl);
    fflush(stdout);
}

int load_args(int argc, char **argv)
{
    arguments.secs = 0;
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
        else
        { // se for o argumento fifoname
            arguments.fifoname = *argv;
        }
    }
    if (arguments.secs == 0)
    {
        printf("ERRO nos Parametros!\n");
        return 1;
    }
    return 0;
}

int max = 100;
int init(int argc, char **argv)
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);
    if (load_args(argc, argv))
        return 1;
    start = time(NULL);
    return 0;
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
    if (init(argc, argv))
        exit(1);

    //abre a fifo pública
    fifo = open(arguments.fifoname, O_WRONLY);

    //loop principal
    fflush(stdout);
    double t = 0;
    while ((t = timeSinceStarttime()) / 1000 <= arguments.secs && out)
    {
        msleep(1);
        pthread_t t;
        int err;
        pthread_mutex_lock(&t_queue);
        threads++;
        if (threads > 5000)
            pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);

	if(timeSinceStarttime() / 1000 <= arguments.secs){
        	while ((err = pthread_create(&t, NULL, utilizador, NULL)))
        	{
        	    msleep(1);
        	}
        	if (i > u)
        	    u = i;
	}
	else
		break;
    }

    while (threads>1)
    {
	pthread_mutex_lock(&t_queue);
        	pthread_cond_wait(&tvar, &t_queue);
        pthread_mutex_unlock(&t_queue);
    }

    close(fifo);
    free(startTime);

    exit(0);
}

void *utilizador()
{
    //gera tempo aleatório
    unsigned seed = time(NULL) + i;
    int dur = rand_r(&seed) % 99 + 1;

    //incrementa o i, apenas um pode aceder de cada vez
    pthread_mutex_lock(&add_i);
    i++;
    pthread_mutex_unlock(&add_i);

    //cria a struct request que vai ser enviada para o fifo
    request tmp = {i, getpid(), gettid(), dur, -1};

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


    if (timeSinceStarttime() / 1000 >= arguments.secs)
    {
        if (unlink(fifo_name))
            fprintf(stderr, "Erro (com '%s'): %s\n", fifo_name, strerror(errno));

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
    else if (!file_exists(arguments.fifoname) || write(fifo, &tmp, sizeof(request)) == -1)
    {
        //printCLOSD(&tmp);

        if (unlink(fifo_name))
            fprintf(stderr, "Erro (com '%s'): %s\n", fifo_name, strerror(errno));

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
    else{
	printIWANT(&tmp);
	fflush(stdout);
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
        fprintf(stderr, "Erro (com '%s'): %s\n", fifo_name, strerror(errno));

    if (close(private_fifo))
        fprintf(stderr, "Erro :%s\n", strerror(errno));

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
