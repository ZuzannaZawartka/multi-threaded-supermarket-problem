#include "cashier.h"
#include <semaphore.h>  //  sem_t


#define MAX_CLIENTS 100

#define K 10  // Na każdych 10 klientów przypada 1 czynna kasa


#define SEM_KEY 1234
#define SHM_KEY 5678

typedef struct {
    Cashier cashiers[MAX_CASH_REGISTER];  // 10 kas
    int active_clients;                     // Liczba aktywnych klientów
    sem_t sem;                              // Semafor do synchronizacji
} SharedData;


void initialize_shared_memory();  // Inicjalizuje pamięć dzieloną
SharedData* get_shared_data();   // zwraca pointer do shared data
void cleanup_shared_memory();   // Aktualizuje ogólne informacje o sklepie
void update_supermarket_info(SharedData *shared_data);