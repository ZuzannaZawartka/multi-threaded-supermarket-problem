#ifndef CASHIER_H
#define CASHIER_H

#include <pthread.h>

typedef struct {
    long mtype;  // Typ komunikatu (ID kasjera)
    pid_t customer_pid;  // PID klienta
} Message;


#define MAX_CASHIERS 10

void init_cashier(int cashier_id);
void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers);
void cleanup_queue(int cashier_id) ;
void* cashier_function(void* arg);
void wait_for_cashiers(pthread_t* cashier_threads, int num_cashiers);

#endif
