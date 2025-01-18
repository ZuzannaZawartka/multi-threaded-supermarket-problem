#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <sys/types.h>
#include <semaphore.h>

typedef struct {
    int cashier_id;  // Numer kasjera
    pid_t pid;       // PID klienta
} CustomerData;

void* customer_function(); 
void* create_customer_processes(void* arg) ;//funkcja tworząca proces klienta
void wait_for_customers();//czekanie aż każdy proces zakończy się
void terminate_all_customers();//zamknięcie wszystkich procesów
int get_customers_in_shop();

void init_semaphore_customer();
void destroy_semaphore_customer();
sem_t* get_semaphore_customer();

int safe_sem_wait();
int safe_sem_post();

int generate_random_time(float min_seconds, float max_seconds);
void handle_customer_creating_signal_fire(int sig) ;
void handle_customer_signal(int sig, siginfo_t *info, void *ucontext);
#endif
