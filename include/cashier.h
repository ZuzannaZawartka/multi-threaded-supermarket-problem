#ifndef CASHIER_H
#define CASHIER_H

#include <pthread.h>
#include "manager_cashiers.h"

typedef struct {
    long mtype;  // Typ komunikatu (ID kasjera)
    pid_t customer_pid;  // PID klienta
} Message;


#define MAX_CASHIERS 10


void create_cashier(pthread_t* cashier_thread, int* cashier_id);
void cleanup_queue(int cashier_id) ;
void* cashier_function(void* arg);

void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers);
void handle_cashier_signal(int sig);
void cleanup_all_queues() ;


#endif
