#ifndef TYPES_H
#define TYPES_H
#include <sys/types.h>

#define MAX_THREAD 1020

struct{
    int i;
    pid_t pid;
    pid_t tid;
    float dur;
    int pl;
}typedef request;

#endif // TYPES_H