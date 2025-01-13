#ifndef CASHIER_MANAGER_H
#define CASHIER_MANAGER_H

#include <pthread.h>
#include <signal.h>



// Funkcja menedżera kasjerów, która monitoruje liczbę klientów
void* manage_customers(void* arg);

// Funkcja do inicjalizacji menedżera kasjerów
void init_manager(pthread_t* manager_thread);

// Funkcja do zakończenia wątku menedżera kasjerów
void terminate_manager(pthread_t manager_thread);

void terminate_cashiers(pthread_t* cashier_threads, int current_cashiers, pthread_t manager_thread);

// Zmień deklarację funkcji w manager_cashiers.h
void create_initial_cashiers();


void sigint_handler(int signum) ;

void increment_cashiers();

void decrement_cashiers();

int get_current_cashiers();

void send_signal_to_cashiers() ;

pthread_t get_cashier_thread(pthread_t* cashier_threads, int index) ;


#endif
