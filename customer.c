#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/sem.h>


void visit_supermarket() {
    srand(time(NULL)); 
    // Losowy czas spędzony w supermarkecie ( między 1 a 10 sekund)
    int visit_time = rand() % 10 + 1;
    printf("Klient: Wchodzę do supermarketu na %d sekund.\n", visit_time);
    sleep(visit_time);
    printf("Klient: Wychodzę z supermarketu.\n");
}

