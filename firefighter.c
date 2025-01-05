#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void send_fire_alarm() {
    sleep(5);
    printf("Strażak: Wysłałem sygnał o pożarze!\n");
}

