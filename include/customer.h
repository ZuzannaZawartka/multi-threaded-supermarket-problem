#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>
#include <semaphore.h>


#define MAX_CUSTOMERS 100  // Maksymalna liczba klientów

typedef struct {
    int cashier_id;  // Numer kasjera
    pid_t pid;       // PID klienta
} CustomerData;

void* customer_function(void* arg); 

//funkcja tworząca proces klienta
void* create_customer_processes(void* arg) ;


//czekanie aż każdy proces zakończy się
void wait_for_customers() ;



//zamknięcie wszystkich procesów
void terminate_all_customers();

//generowanie czasu randomowego z zakresu min - max
int generate_random_time(int min_time, int max_time);

void handle_customer_signal(int sig);


void init_semaphore_customer();
void destroy_semaphore_customer();

sem_t* get_semaphore_customer();

int get_customers_in_shop();


int safe_sem_wait();
int safe_sem_post();


#endif
