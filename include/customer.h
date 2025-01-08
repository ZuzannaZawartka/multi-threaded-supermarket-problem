#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>

#define MAX_CUSTOMERS 100  // Maksymalna liczba klientów

extern int queue_ids[];  // Tablica kolejki komunikatów dla kasjerów
//TO DO może przerobić jako pamięć współdzielona

typedef struct {
    int cashier_id;  // Numer kasjera
    pid_t pid;       // PID klienta
} CustomerData;

void* customer_function(void* arg); 

//funkcja tworząca proces klienta
void create_customer_processes(int num_customers, int num_cashiers);

//czekanie aż każdy proces zakończy się
void wait_for_customers(int num_customers);

//zamknięcie wszystkich procesów
void terminate_all_customers();

#endif
