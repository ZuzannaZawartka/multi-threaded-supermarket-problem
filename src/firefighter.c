#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "creator_customer.h"           //funkcja generate_random_time
#include "main.h"                       //dla zmiennych MIN_WAIT_FIREFIGHTER_TIME

// Kolory dla wyświetlanych komunikatów
const char* orange = "\033[38;5;214m";  // ANSI dla pomarańczowego
const char* reset = "\033[0m";          // Reset kolorów

/**
 * @brief Obsługuje sygnał SIGQUIT wysłany do strażaka.
 * 
 * @details Funkcja obsługuje sygnał SIGQUIT, który powoduje zakończenie wątku strażaka.
 * Używa `pthread_exit` do zakończenia wątku.
 * 
 * @param signum Numer sygnału (oczekiwany SIGQUIT).
 */
void firefighter_sigint_handler(int signum) {
    printf("Strażak wychodzi \n");
    pthread_exit(NULL); // Bezpośrednie zakończenie wątku
}

/**
 * @brief Wyświetla odliczanie od 5 do 0.
 * 
 * @details Funkcja symuluje ostrzeżenie o trwającym pożarze, wyświetlając komunikaty z odliczaniem.
 * Używa kolorów ANSI do wyróżnienia komunikatów.
 */
void countdown_to_exit() {
    for (int i = 5; i > 0; i--) {
        printf("%sPożar trwa! [%d]%s\n", orange, i, reset);
    }
}

/**
 * @brief Funkcja główna wykonywana przez strażaka.
 * 
 * @details Funkcja oczekuje losowy czas w milisekundach (zdefiniowany przez 
 * `MIN_WAIT_FIREFIGHTER_TIME` i `MAX_WAIT_FIREFIGHTER_TIME`), a następnie wysyła 
 * sygnał `SIGINT` do wszystkich procesów potomnych w grupie. Obsługuje także sygnał 
 * `SIGQUIT`, który kończy działanie wątku strażaka.
 */
void firefighter_process() {
    signal(SIGQUIT, firefighter_sigint_handler);
   
    // Losowanie czasu oczekiwania
    int random_time = generate_random_time(MIN_WAIT_FIREFIGHTER_TIME, MAX_WAIT_FIREFIGHTER_TIME );  // Losowanie z zakresu 50-100 sekund
    printf("Strażak czeka na %d milisekund...\n",random_time);

    // Oczekiwanie przez losowy czas
    usleep(random_time);

    // Wysyłanie sygnału SIGINT do wszystkich procesów potomnych głównego procesu
    printf("Strażak wysyła SIGINT do wszystkich procesów potomnych...\n");

    // Wysłanie sygnału SIGINT do wszystkich procesów potomnych
    kill(0, SIGINT);  // 0 oznacza, że wysyłamy do wszystkich procesów potomnych w grupie
}

/**
 * @brief Inicjalizuje wątek strażaka.
 * 
 * @param firefighter_thread Wskaźnik do wątku strażaka.
 * 
 * @details Tworzy nowy wątek dla strażaka, który wykonuje funkcję `firefighter_process`.
 * W przypadku błędu zakończenia działania programu.
 */
void init_firefighter(pthread_t *firefighter_thread) {
    if (pthread_create(firefighter_thread, NULL, (void*)firefighter_process, NULL) != 0) {
        perror("Błąd tworzenia wątku dla strażaka");
        exit(1);
    }else{
        printf("Wątek strażaka kasjerów utworzony, id wątku: %ld\n", *firefighter_thread);
    }
}

/**
 * @brief Oczekuje na zakończenie wątku strażaka.
 * 
 * @param firefighter_thread Wątek strażaka.
 * 
 * @details Funkcja używa `pthread_join` do synchronizacji z zakończeniem działania 
 * wątku strażaka. W przypadku błędu wypisuje komunikat diagnostyczny i kończy program.
 */
void wait_for_firefighter(pthread_t firefighter_thread) {
    // Czekanie na zakończenie wątku strażaka
    if (pthread_join(firefighter_thread, NULL) != 0) {
        perror("Błąd oczekiwania na wątek strażaka");
        exit(1);
    }
}