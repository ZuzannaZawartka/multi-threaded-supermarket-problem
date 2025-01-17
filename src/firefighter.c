#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>


// Handler sygnału SIGINT dla strażaka
void firefighter_sigint_handler(int signum) {
    // printf("Strażak otrzymał sygnał SIGINT! Kończę działanie...\n");
    pthread_exit(NULL); // Bezpośrednie zakończenie wątku
}


// Funkcja odliczania od 5 do 0
void countdown_to_exit() {
    for (int i = 5; i > 0; i--) {
        printf("Pożar trwa! [%d]\n", i);
        sleep(1);  // Czeka 1 sekundę
    }
}

void firefighter_process() {
    signal(SIGQUIT, firefighter_sigint_handler);
    // // int random_time = rand() % 31 + 20;  // Losowanie z zakresu 20-50
    // int random_time = rand() % 51 + 50;  // Losowanie z zakresu 50-100 sekund
    // printf("Strażak czeka na %d sekund...\n",random_time);

    
    // sleep(random_time);  // Czeka 5 sekund

    // // Wysyłanie sygnału SIGINT do wszystkich procesów potomnych głównego procesu
    // printf("Strażak wysyła SIGINT do wszystkich procesów potomnych...\n");

    // // Wysłanie sygnału SIGINT do wszystkich procesów potomnych
    // kill(0, SIGINT);  // 0 oznacza, że wysyłamy do wszystkich procesów potomnych w grupie
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
