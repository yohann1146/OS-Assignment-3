#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// Shared Variables
int shared_resource = 0;
int read_count = 0;
int write_count = 0;

// Semaphores (used instead of mutexes)
sem_t rmutex;          // Protects read_count
sem_t wmutex;          // Protects write_count
sem_t resource_access; // Ensures mutual exclusion for the shared resource
sem_t read_try;        // Enforces writer priority by blocking readers when a writer is waiting

// Helper function to get random microseconds
int get_random_us(int max_seconds, int max_ms) {
    int max_us = (max_seconds * 1000000) + (max_ms * 1000);
    if (max_us == 0) return 0;
    return rand() % max_us;
}

void* reader(void* arg) {
    int id = *((int*)arg);
    
    while (1) {
        // --- Entry Section ---
        sem_wait(&read_try);       // Wait if a writer is waiting/writing
        sem_wait(&rmutex);         // Lock to update read_count
        
        read_count++;
        if (read_count == 1) {
            sem_wait(&resource_access); // First reader locks the resource from writers
        }
        
        sem_post(&rmutex);         // Unlock read_count
        sem_post(&read_try);       // Allow the next reader to try entering

        // --- Critical Section (Reading) ---
        printf("[Reader %d] Reading resource: %d (Active Readers: %d)\n", id, shared_resource, read_count);
        usleep(get_random_us(1, 0)); // Simulate reading (up to 1 sec)

        // --- Exit Section ---
        sem_wait(&rmutex);         // Lock to update read_count
        read_count--;
        if (read_count == 0) {
            sem_post(&resource_access); // Last reader releases the resource for writers
        }
        sem_post(&rmutex);         // Unlock read_count

        // --- Remainder Section ---
        usleep(get_random_us(3, 0)); // Sleep before trying to read again (up to 3 secs)
    }
    return NULL;
}

void* writer(void* arg) {
    int id = *((int*)arg);
    
    while (1) {
        // --- Entry Section ---
        sem_wait(&wmutex);         // Lock to update write_count
        write_count++;
        if (write_count == 1) {
            sem_wait(&read_try);   // First writer blocks any NEW readers from entering
        }
        sem_post(&wmutex);         // Unlock write_count

        // --- Critical Section (Writing) ---
        sem_wait(&resource_access); // Lock the resource for exclusive access
        
        shared_resource += 5;
        printf("[Writer %d] Writing to resource: %d\n", id, shared_resource);
        usleep(get_random_us(1, 500)); // Simulate writing (up to 1.5 secs)
        
        sem_post(&resource_access); // Release the resource

        // --- Exit Section ---
        sem_wait(&wmutex);         // Lock to update write_count
        write_count--;
        if (write_count == 0) {
            sem_post(&read_try);   // Last writer allows readers to try entering again
        }
        sem_post(&wmutex);         // Unlock write_count

        // --- Remainder Section ---
        usleep(get_random_us(5, 0)); // Sleep before trying to write again (up to 5 secs)
    }
    return NULL;
}

int main() {
    srand(time(NULL));

    // Initialize semaphores to 1 (acting as binary semaphores/mutexes)
    sem_init(&rmutex, 0, 1);
    sem_init(&wmutex, 0, 1);
    sem_init(&resource_access, 0, 1);
    sem_init(&read_try, 0, 1);

    pthread_t readers[4];
    pthread_t writers[2];
    int reader_ids[4];
    int writer_ids[2];

    // Spawn exactly 4 reader threads
    for (int i = 0; i < 4; i++) {
        reader_ids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader, &reader_ids[i]);
    }

    // Spawn exactly 2 writer threads
    for (int i = 0; i < 2; i++) {
        writer_ids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer, &writer_ids[i]);
    }

    // Join threads (this loop runs infinitely in this simulation)
    for (int i = 0; i < 4; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }

    // Cleanup
    sem_destroy(&rmutex);
    sem_destroy(&wmutex);
    sem_destroy(&resource_access);
    sem_destroy(&read_try);

    return 0;
}
