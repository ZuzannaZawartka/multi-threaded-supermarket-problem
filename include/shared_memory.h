#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>
#include <semaphore.h>
#include "main.h"

// Struktura pamięci dzielonej
typedef struct {
    int queue_ids[MAX_CASHIERS];           // Tablica identyfikatorów kolejek komunikatów
    int active_cashiers;                  // Liczba aktywnych kasjerów dostępnych dla klientów (takich do których można ustawić się w kolejce)
    int customer_count;                   // Aktualna liczba klientów w sklepie
    int fire_flag;                        // Flaga określająca, czy wystąpił pożar
    int fire_handling_complete;           // Flaga określająca, czy zakończono obsługę klientów po pożarze
} SharedMemory;

// Zarządzanie pamięcią dzieloną
SharedMemory* init_shared_memory();              // Inicjalizacja pamięci dzielonej
SharedMemory* get_shared_memory();               // Pobranie wskaźnika do istniejącej pamięci dzielonej
void cleanup_shared_memory(SharedMemory* shared_mem); // Czyszczenie pamięci dzielonej

// Funkcje do zarządzania tablicą kolejek komunikatów
void set_queue_id(SharedMemory* shared_mem, int cashier_id, int queue_id); // Ustawienie kolejki komunikatów dla kasjera
int get_queue_id(SharedMemory* shared_mem, int cashier_id);                // Pobranie identyfikatora kolejki dla kasjera

// Zarządzanie semaforem dla pamięci dzielonej
void cleanup_semaphore();                     // Czyszczenie semafora pamięci dzielonej
sem_t* get_semaphore();                       // Pobranie semafora dla pamięci dzielonej

// Zarządzanie liczbą aktywnych kasjerów (czyli takich do których można ustawić sie w kolejce)
int get_active_cashiers(SharedMemory* shared_mem);         // Pobranie liczby aktywnych kasjerów
void decrement_active_cashiers(SharedMemory* shared_mem);  // Zmniejszenie liczby aktywnych kasjerów
void increment_active_cashiers(SharedMemory* shared_mem);  // Zwiększenie liczby aktywnych kasjerów

// Zarządzanie liczbą klientów
void decrement_customer_count(SharedMemory* shared_mem);   // Zmniejszenie liczby klientów
void increment_customer_count(SharedMemory* shared_mem);   // Zwiększenie liczby klientów
int get_customer_count(SharedMemory* shared_mem);          // Pobranie liczby klientów

// Zarządzanie flagą pożaru
void set_fire_flag(SharedMemory* shared_mem, int fire_status); // Ustawienie flagi pożaru
int get_fire_flag(SharedMemory* shared_mem);                  // Pobranie flagi pożaru

// Zarządzanie zakończeniem obsługi klientów po pożarze
void set_fire_handling_complete(SharedMemory* shared_mem, int status); // Ustawienie flagi zakończenia obsługi po pożarze
int get_fire_handling_complete(SharedMemory* shared_mem);             // Pobranie flagi zakończenia obsługi po pożarze

#endif
