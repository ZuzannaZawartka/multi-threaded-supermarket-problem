#ifndef FIREFIGHTER_H
#define FIREFIGHTER_H

#include <pthread.h>

void *firefighter_process(void *arg); 
void init_firefighter(pthread_t *firefighter);  
void countdown_to_exit();
void firefighter_sigint_handler(int signum);
#endif
