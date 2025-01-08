#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <pthread.h>

extern int queue_ids[];  // Tablica kolejki komunikatów dla kasjerów
//TO DO może przerobić jako pamięć współdzielona

typedef struct {
    int cashier_id;  // Numer kasjera
    pid_t pid;       // PID klienta
} CustomerData;

void* customer_function(void* arg); 

#endif
