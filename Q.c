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

pthread_mutex_t add_i = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_fifo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t add_queue = PTHREAD_MUTEX_INITIALIZER;
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
        perror("ERROR setting up FIFO on main() of U.c ");
        //exit(errno);
    }
}

void *processRequest();
int i = 0;
int fifo;
request input;

int main(int argc, char **argv)
{
    init(argc, argv);
    pthread_t t;
    printf("Tentou\n");
    fifo = open(arguments.fifoname, O_RDONLY); //abre a fifo pública
    printf("Opened\n");
    double ti;

    while (((ti = timeSinceStartTime()) / 1000) < arguments.secs)
    {
        if (read(fifo, &input, sizeof(input)))
        {
            if (((input.dur + ti) / 1000) < arguments.secs)
            {
                input.pl = i;
                spots[i] = t;
                if (!(i < maxFreeSpots))
                {
                    pthread_join(spots[i], NULL);
                }
            }
            printf("OK - (Q.c) % i %i %i %f %i\n", input.i, input.pid, input.tid, input.dur, input.pl);
            pthread_create(&t, NULL, processRequest, NULL);
            i++;
            if (i > maxFreeSpots)
            {
                i %= maxFreeSpots;
            }
        }
    }

    while (read(fifo, &input, sizeof(input)))
    {
        pthread_create(&t, NULL, processRequest, NULL);
    }

    free(startTime);
    free(queue);
    return 0;
}

void *processRequest()
{
    FILE *tmp;
    char fifoLoad[25];
    sprintf(fifoLoad, "%d.%d", input.pid, input.tid);
    tmp = fopen(fifoLoad, "w");

    if (input.pl == -1)
    {
        msleep(input.dur);
    }

    // fflush(stdout);
    // printf("OK - (Q.c) % i %i %i %f %i\n", input.i, input.pid, input.tid, input.dur, input.pl);
    // printf("OK - (Q.c) % i %i %i %f %i 1---%f\n", input.i, input.pid, input.tid, input.dur, input.pl, timeSinceStartTime());
    // fflush(stdout);

    fwrite(&input, 1, sizeof(request) + 1, tmp);

    // fflush(stdout);
    // printf("OK - (Q.c) % i %i %i %f %i 2---%f\n", input.i, input.pid, input.tid, input.dur, input.pl, timeSinceStartTime());
    // fflush(stdout);

    // write(fifo, &input, sizeof(request));

    // THE FOLLOWING NEEDS TO BE MODIFIED - IT IS ACTING MERELY AS A PLACEHOLDER SO THE CODE DOESN'T BLOCK
    char stringRet[10] = "Okay";
    write(fifo, stringRet, strlen(stringRet) + 1);

    fclose(tmp);

    return NULL;
}
