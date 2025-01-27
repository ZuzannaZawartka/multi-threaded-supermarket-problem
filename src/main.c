#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>   
#include <errno.h>   
#include "creator_customer.h"
#include "shared_memory.h"
#include "manager_cashiers.h"
#include "firefighter.h"
#include "cashier.h"

// Deklaracja pthread_tryjoin_np
int pthread_tryjoin_np(pthread_t thread, void **retval);

SharedMemory* shared_mem = NULL;
pthread_t monitor_thread;  // Declare it as a global variable
pthread_t customer_thread;
pthread_t firefighter_thread; // Wątek strażaka
pthread_t cleanup_thread; // Wątek strażaka

extern pthread_mutex_t pid_mutex;  // Mutex dla ochrony tablicy PID-ów
extern pid_t pids[MAX_CUSTOMERS];  // Tablica przechowująca PIDs procesów potomnych
extern int created_processes ; // Licznik stworzonych procesów

extern pthread_t cashier_threads[MAX_CASHIERS];

void mainHandlerProcess(int signum) {

    if(get_fire_flag(shared_mem)==1){
        return;
    }

    set_fire_flag(shared_mem,1); 

    countdown_to_exit();

}

int main() {
    srand(time(NULL));  
    init_semaphore_customer();
    shared_mem = init_shared_memory();

    // Set process group for signal handling
    set_process_group();
    signal(SIGINT, mainHandlerProcess);// Rejestracja handlera SIGINT dla pożaru


    init_manager(&monitor_thread);  // Tworzenie wątku dla managera kasjerow

    // Tworzymy wątek do czyszczenia zakończonych procesów
    if (pthread_create(&cleanup_thread, NULL, cleanup_processes, NULL) != 0) {
        perror("Błąd przy tworzeniu wątku czyszczącego");
        exit(1);
    }

  printf("Przed while\n");
      // Main loop to fork customer processes
    while (!get_fire_flag(shared_mem)) {
  
        if (safe_sem_wait() == -1) {  // Wait for space to create new customers
            perror("Błąd podczas oczekiwania na semafor");
            break;
        }
        increment_customer_count(shared_mem);

        pid_t pid = fork();  // Fork a new process for the customer
        if (pid < 0) {
            perror("Fork failed");
            safe_sem_post();  // Release semaphore if fork fails
        } else if (pid == 0) {
            // Child process: Execute customer program
            if (execl("./src/customer", "customer", (char *)NULL) == -1) {
                perror("Błąd wykonania programu customer");
                exit(1);
            }
        } else {
            if (get_fire_flag(shared_mem)) {  // Sprawdź, czy proces ma się zakończyć
                kill(pid, SIGKILL);
            }else{
                pthread_mutex_lock(&pid_mutex);
                pids[created_processes++] = pid;  // Save the child PID
                pthread_mutex_unlock(&pid_mutex);
            }
     
        }
              int min_time = 1000000;  // 0.5 sekundy w mikrosekundach
        int max_time = 2000000; // 1 sekunda w mikrosekundach
        int random_time = min_time + rand() % (max_time - min_time + 1);

        // Uśpienie na losowy czas
            // usleep(random_time);
        // usleep(200000);  // Opóźnienie przed kolejnym sprawdzeniem zakończonych procesów
    }
  
    set_fire_handling_complete(shared_mem,1);
    

    pthread_join(cleanup_thread, NULL);
    send_signal_to_cashiers(SIGHUP); //wysyłamy sygnał

    wait_for_cashiers(cashier_threads,get_current_cashiers()); //czekamy na zakończenie 
   

    wait_for_manager(monitor_thread); //usuniecie manadżera


    cleanAfterCashiers();

    cleanup_shared_memory(shared_mem); //czyszczenie pamięci dzielonej i jej semafora
   
    destroy_semaphore_customer();
    destroy_mutex();
    destroy_pid_mutex();
    printf("KONIEC\n");
    return 0;
}

void send_signal_to_manager(int signal) {
    if (pthread_kill(monitor_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

void send_signal_to_customers(int signal) {
    if (pthread_kill(customer_thread, signal) != 0) {
            perror("Błąd wysyłania sygnału do kasjera");
    }
}

void send_signal_to_firefighter(int signal) {
    if (pthread_kill(firefighter_thread, signal) != 0) {
        perror("Błąd wysyłania sygnału do strażaka"); //(TOFIX ?)tutaj blad wyrzuci gdy strazak sam sie wywola
    }
}

void set_process_group() {
    pid_t pid = getpid();
     if (setpgid(pid, pid) == -1) {// Ustawienie głównego procesu jako lidera grupy procesów (po to żeby strażak mógł wysłać poprawnie sygnały)
        perror("Błąd przy ustawianiu grupy procesów");
        exit(1);  // Zakończenie programu w przypadku błędu
    } 
}
