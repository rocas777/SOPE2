#ifndef TYPES_H
#define TYPES_H
#include <sys/types.h>
struct{
    int i;
    pid_t pid;
    pid_t tid;
    float dur;
    int pl;
}typedef request;

#endif // TYPES_H
