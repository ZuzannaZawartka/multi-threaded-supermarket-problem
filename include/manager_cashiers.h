#ifndef CASHIER_MANAGER_H
#define CASHIER_MANAGER_H

#include <pthread.h>
#include <signal.h>
#include "shared_memory.h"

// Zarządzanie menedżerem kasjerów
void* manage_customers(void* arg);                    // Funkcja wątku zarządzającego kasjerami
void init_manager(pthread_t* manager_thread);         // Inicjalizacja menedżera kasjerów
void wait_for_manager(pthread_t manager_thread);      // Oczekiwanie na zakończenie wątku menedżera


void create_initial_cashiers();                       // Utworzenie minimalnej liczby kasjerów
void terminate_cashiers(pthread_t* cashier_threads, 
                        int current_cashiers, 
                        pthread_t manager_thread);    // Zakończenie pracy kasjerów

// Funkcja obsługująca sygnał SIGTERM (pożar)
void fire_sigTermHandler(int signum);                 // Obsługa sygnału pożaru

// Zarządzanie liczbą kasjerów
void increment_cashiers();                            // Zwiększ liczbę kasjerów
void decrement_cashiers();                            // Zmniejsz liczbę kasjerów
int get_current_cashiers();                           // Pobierz bieżącą liczbę kasjerów
void send_signal_to_cashiers();                       // Wysyłanie sygnału do wszystkich kasjerów

// Zarządzanie wątkami i ID kasjerów
pthread_t get_cashier_thread(pthread_t* cashier_threads, int index); // Pobierz wątek kasjera
int get_cashier_id(int* cashier_ids, int index);                     // Pobierz ID kasjera
void set_cashier_id(int* cashier_ids, int index, int id);            // Ustaw ID kasjera
void set_cashier_thread(pthread_t* cashier_threads, int index, pthread_t thread); // Ustaw wątek kasjera
int* get_cashier_id_pointer(int* cashier_ids, int index);            // Pobierz wskaźnik do ID kasjera

// Funkcje do wyboru kolejki przez klienta
int get_message_count_for_cashier(int queue_id);                     // Pobierz liczbę wiadomości w kolejce kasjera
int select_cashier_with_fewest_people(SharedMemory* shared_mem);     // Wybierz kasjera z najmniejszą liczbą klientów w kolejce

// Zarządzanie mutexami
void destroy_mutex();                                                // Zniszcz mutex

#endif
