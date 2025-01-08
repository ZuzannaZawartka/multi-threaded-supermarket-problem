#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <pthread.h>

typedef struct {
    int should_exit;  // Flaga zakończenia
    pthread_mutex_t mutex;  // Mutex dla synchronizacji
} SharedMemory;

SharedMemory* init_shared_memory();
void cleanup_shared_memory(SharedMemory* shared_mem);
void set_should_exit(SharedMemory* shared_mem, int value);
int get_should_exit(SharedMemory* shared_mem);

#endif  // SHARED_MEMORY_H
