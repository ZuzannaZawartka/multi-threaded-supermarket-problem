#ifndef FIREFIGHTER_H
#define FIREFIGHTER_H

#include <pthread.h>

void *firefighter_process(void *arg);                     // Główna funkcja wątku strażaka
void init_firefighter(pthread_t *firefighter);            // Inicjalizacja wątku strażaka
void countdown_to_exit();                                 // Odliczanie przed zakończeniem pracy przez strażaka
void firefighter_sigint_handler(int signum);              // Obsługa sygnału SIGINT przez strażaka
void wait_for_firefighter(pthread_t firefighter_thread);  // Czekanie na zakończenie wątku strażaka

#endif
