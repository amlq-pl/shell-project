#ifndef _SIGHANDLER_H
#define _SIGHANDLER_H

#include <sys/types.h>
void sigchldhandler(int);
void siginthandler(int);
#endif

