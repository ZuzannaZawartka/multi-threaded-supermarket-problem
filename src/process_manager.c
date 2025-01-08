#include "process_manager.h"
#include <stdio.h>
#include <stdlib.h>

// Lista powiązań przechowująca pidy procesów
ProcessNode* process_list = NULL;  


void add_process(pid_t pid) {

    if (process_exists(pid)) {
        printf("PID %d już istnieje w liście\n", pid);
        return;
    }

    ProcessNode* new_node = (ProcessNode*)malloc(sizeof(ProcessNode));
    if (new_node == NULL) {
        perror("Błąd alokacji pamięci");
        exit(1);
    }

    new_node->pid = pid;
    new_node->next = process_list;
    process_list = new_node;
}

void remove_process(pid_t pid) {
    ProcessNode* current = process_list;
    ProcessNode* prev = NULL;
    
    // Szukamy PID-a w liście
    while (current != NULL && current->pid != pid) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        printf("PID %d nie znaleziono w liście\n", pid);
        return; 
    }

    if (prev == NULL) {
        process_list = current->next;  
    } else {
        prev->next = current->next; 
    }

    free(current); 
}

int process_exists(pid_t pid) {
    ProcessNode* current = process_list;
    while (current != NULL) {
        if (current->pid == pid) {
            return 1; 
        }
        current = current->next;
    }
    return 0; 
}


void cleanup_processes() {
    ProcessNode* current = process_list;

    while (current != NULL) {
        ProcessNode* next = current->next; 
        free(current); 
        current = next;
    }

    process_list = NULL;  
    printf("Lista procesów została oczyszczona.\n");
}
