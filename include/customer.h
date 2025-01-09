#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>

#define MAX_CUSTOMERS 100  // Maksymalna liczba klientów

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

//generowanie czasu randomowego z zakresu min - max
int generate_random_time(int min_time, int max_time);


#endif
