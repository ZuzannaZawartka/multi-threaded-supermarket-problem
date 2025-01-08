#ifndef CASHIER_H
#define CASHIER_H

#include <pthread.h>

typedef struct {
    long mtype;  // Typ komunikatu (ID kasjera)
    pid_t customer_pid;  // PID klienta
} Message;


#define MAX_CASHIERS 10
extern int queue_ids[MAX_CASHIERS];  // Tablica dla kolejki komunikatów

void init_cashier(int cashier_id);
void init_cashiers(pthread_t* cashier_threads, int* cashier_ids, int num_cashiers);
void* cashier_function(void* arg);

#endif
