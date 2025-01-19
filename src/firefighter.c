#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "customer.h" //funkcja generate_random_time
#include "main.h" //dla zmiennych MIN_WAIT_FIREFIGHTER_TIME

const char* orange = "\033[38;5;214m"; // ANSI escape code for orange
const char* reset = "\033[0m";  // Reset kolorów

// Handler sygnału SIGINT dla strażaka
void firefighter_sigint_handler(int signum) {
    pthread_exit(NULL); // Bezpośrednie zakończenie wątku
}

// Funkcja odliczania od 5 do 0
void countdown_to_exit() {
    for (int i = 5; i > 0; i--) {
        printf("%sPożar trwa! [%d]%s\n", orange, i, reset);
        sleep(1); 
    }
}

void firefighter_process() {
    signal(SIGQUIT, firefighter_sigint_handler);
   
    int random_time = generate_random_time(MIN_WAIT_FIREFIGHTER_TIME, MAX_WAIT_FIREFIGHTER_TIME );  // Losowanie z zakresu 50-100 sekund
    printf("Strażak czeka na %d milisekund...\n",random_time);

    usleep(random_time);  // Czeka 5 sekund

    // Wysyłanie sygnału SIGINT do wszystkich procesów potomnych głównego procesu
    printf("Strażak wysyła SIGINT do wszystkich procesów potomnych...\n");

    // Wysłanie sygnału SIGINT do wszystkich procesów potomnych
    kill(0, SIGINT);  // 0 oznacza, że wysyłamy do wszystkich procesów potomnych w grupie
}

// Funkcja inicjująca strażaka
void init_firefighter(pthread_t *firefighter_thread) {
    if (pthread_create(firefighter_thread, NULL, (void*)firefighter_process, NULL) != 0) {
        perror("Błąd tworzenia wątku dla strażaka");
        exit(1);
    }else{
        printf("Wątek strażaka kasjerów utworzony, id wątku: %ld\n", *firefighter_thread);
    }
}

void wait_for_firefighter(pthread_t firefighter_thread) {
    // Czekanie na zakończenie wątku strażaka
    if (pthread_join(firefighter_thread, NULL) != 0) {
        perror("Błąd oczekiwania na wątek strażaka");
        exit(1);
    } else {
        printf("Strażak zakończył swoją pracę.\n");
    }
}