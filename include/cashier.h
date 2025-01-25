#ifndef CASHIER_H
#define CASHIER_H

#include <pthread.h>
#include "manager_cashiers.h"

//struktura komunikatu kolejki
typedef struct {
    long mtype;  // Typ komunikatu (ID kasjera)
    pid_t customer_pid;  // PID klienta
} Message;

void create_cashier(pthread_t* cashier_thread, int* cashier_id);
void cleanup_queue(int cashier_id) ;
void* cashier_function(void* arg);

void wait_for_cashiers(pthread_t* cashier_threads);
void handle_cashier_signal_fire();
void cleanAfterCashiers();
void closeCashier(int signum) ;

void block_signal_SIGTERM();
#endif
