#ifndef CASHIER_MANAGER_H
#define CASHIER_MANAGER_H

#include <pthread.h>

// Funkcja menedżera kasjerów, która monitoruje liczbę klientów
void* manage_customers(void* arg);

// Funkcja do inicjalizacji menedżera kasjerów
void init_manager(pthread_t* manager_thread);

// Funkcja do zakończenia wątku menedżera kasjerów
void terminate_manager(pthread_t manager_thread);

#endif
