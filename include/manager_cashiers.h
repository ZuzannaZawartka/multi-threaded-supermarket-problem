#ifndef CASHIER_MANAGER_H
#define CASHIER_MANAGER_H

#include <pthread.h>
#include <signal.h>
// #include "main.h"
#include "shared_memory.h"

//zarządzanie manadżerem kasjerów
void* manage_customers(void* arg);
void init_manager(pthread_t* manager_thread);
void wait_for_manager(pthread_t manager_thread);

//inicjowanie kasjerów MIN 2
void create_initial_cashiers();
void terminate_cashiers(pthread_t* cashier_threads, int current_cashiers, pthread_t manager_thread);

//funkcja na sygnał SIGTERM o pożarze
void fire_sigTermHandler(int signum) ;

//zarządzanie zmienna dzielona między wątki o ilości kasjerów
void increment_cashiers();
void decrement_cashiers();
int get_current_cashiers();
void send_signal_to_cashiers() ;

//zarzadzanie wątkami oraz id kasjerów
pthread_t get_cashier_thread(pthread_t* cashier_threads, int index) ;
int get_cashier_id(int* cashier_ids, int index) ;
void set_cashier_id(int* cashier_ids, int index, int id) ;
void set_cashier_thread(pthread_t* cashier_threads, int index, pthread_t thread) ;
int* get_cashier_id_pointer(int* cashier_ids, int index);

//funkcje do wyboru kolejki przez klienta
int get_message_count_for_cashier(int queue_id);
int select_cashier_with_fewest_people(SharedMemory* shared_mem) ;

#endif
