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

struct{
    long unsigned secs;
    char *fifoname;
}typedef args ;

//por alguma razão o gettid não estava definido
pid_t gettid(){
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
int max=100;

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
    queue=malloc(sizeof(pthread_t)*max);
}
int arr_size=0;

void *processRequest();
int i=0;
FILE *fifo;
request input;

int main(int argc, char **argv)
{
    delay(1);   // Atrasa o Q.c para dar tempo ao U.c para criar a FIFO (teremos de usar uma alternativa)
    init(argc,argv);	
    pthread_t t;
    printf("Tentou\n");
    fifo = fopen(arguments.fifoname,"r"); //abre a fifo pública
    if( fifo == NULL ){
        perror("ERROR opening FIFO (at Q.c) ");
        exit(errno); 
    }
    printf("Opened\n");
    while(fread(&input,sizeof(input),1,fifo)){
            printf("OK - (Q.c) % i %i %i %f %i\n",input.i,input.pid,input.tid,input.dur,input.pl); 
            pthread_create(&t,NULL, processRequest,NULL);
            pthread_join(t, NULL);
    }
    free(startTime);
    free(queue);
    return 0;
}

void *processRequest(){
    FILE* tmp;
    char fifoLoad[25];
    sprintf(fifoLoad, "%d.%d", input.pid, input.tid);
    tmp = fopen(fifoLoad, "w");

    // THE FOLLOWING NEEDS TO BE MODIFIED - IT IS ACTING MERELY AS A PLACEHOLDER SO THE CODE DOESN'T BLOCK
        char stringRet[10] = "Okay";
        fwrite(stringRet, strlen(stringRet)+1, 1, fifo);
    
    fclose(tmp);

    return NULL;
}
