#ifndef CREATOR_CUSTOMER_H
#define CREATOR_CUSTOMER_H

#include <sys/types.h> 
#include <semaphore.h>  
#include <fcntl.h>   
#include <sys/stat.h>  
#include <signal.h>   
#include <pthread.h> 

void wait_for_customers();                           // Oczekiwanie na zakończenie procesów klientów
void terminate_all_customers();                      // Zakończenie działania procesów klientów

void init_semaphore_customer();                      // Inicjalizacja semafora klientów
void destroy_semaphore_customer();                   // Usunięcie semafora klientów

sem_t* get_semaphore_customer();                     // Pobranie semafora klientów
int safe_sem_wait();                                 // Bezpieczne oczekiwanie na semafor
int safe_sem_post();                                 // Bezpieczne zwolnienie semafora

int generate_random_time(float min_seconds, float max_seconds);  // Generowanie losowego czasu w mikrosekundach
void handle_customer_creating_signal_fire(int sig);  // Obsługa sygnału zatrzymania tworzenia klientów
int seconds_to_microseconds(float seconds);          // Konwersja sekund na mikrosekundy

void* cleanup_processes(void* arg);                  // Czyszczenie zakończonych procesów klientów
void init_cleanup_thread(pthread_t *cleanup_thread); // Inicjalizacja wątku czyszczącego
void destroy_pid_mutex();                            // Zniszczenie mutexa dla tablicy PID klientów

#endif // CREATOR_CUSTOMER_H
