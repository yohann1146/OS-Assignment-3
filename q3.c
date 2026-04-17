#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// shared variables
int shared_resource = 0;
int read_count = 0;
int write_count = 0;

// semaphores
sem_t rmutex;
sem_t wmutex;
sem_t resource_access;
sem_t read_try;

int get_random_us(int max_seconds, int max_ms) {
    int max_us = (max_seconds * 1000000) + (max_ms * 1000);
    if (max_us == 0) return 0;
    return rand() % max_us;
}

void* reader(void* arg) {
    int id = *((int*)arg);
    
    while (1) {
        // entry section
        sem_wait(&read_try);       //wait
        sem_wait(&rmutex);         // lock to update read_count
        
        read_count++;
        if (read_count == 1) {
            sem_wait(&resource_access); // first reader locks the resource from writers
        }
        
        sem_post(&rmutex);         // unlock read_count
        sem_post(&read_try); 

        //reading
        printf("[Reader %d] Reading resource: %d (Active Readers: %d)\n", id, shared_resource, read_count);
        usleep(get_random_us(1, 0)); //reading

        //exit
        sem_wait(&rmutex);         // lock to update read_count
        read_count--;
        if (read_count == 0) {
            sem_post(&resource_access); // last reader releases the resource for writers
        }
        sem_post(&rmutex);         // unlock read_count

        //remainder
        usleep(get_random_us(3, 0)); // sleep up to 3 secs
    }
    return NULL;
}

void* writer(void* arg) {
    int id = *((int*)arg);
    
    while (1) {
        //enter
        sem_wait(&wmutex);      
        write_count++;
        if (write_count == 1) {
            sem_wait(&read_try);   // first writer blocks new readers from entering
        }
        sem_post(&wmutex);         // unlock write_count

        //write
        sem_wait(&resource_access); // lock the resource
        
        shared_resource += 5;
        printf("[Writer %d] Writing to resource: %d\n", id, shared_resource);
        usleep(get_random_us(1, 500)); // simulate writing (up to 1.5 s)
        
        sem_post(&resource_access); // resouce release

        //exit
        sem_wait(&wmutex);         // Lock to update write_count
        write_count--;
        if (write_count == 0) {
            sem_post(&read_try);   // Last writer allows readers to try entering again
        }
        sem_post(&wmutex);         // Unlock write_count

        usleep(get_random_us(5, 0));
    }
    return NULL;
}

int main() {
    srand(time(NULL));

    sem_init(&rmutex, 0, 1);
    sem_init(&wmutex, 0, 1);
    sem_init(&resource_access, 0, 1);
    sem_init(&read_try, 0, 1);

    pthread_t readers[4];
    pthread_t writers[2];
    int reader_ids[4];
    int writer_ids[2];

    // 4 reader
    for (int i = 0; i < 4; i++) {
        reader_ids[i] = i + 1;
        pthread_create(&readers[i], NULL, reader, &reader_ids[i]);
    }

    // 2 writer
    for (int i = 0; i < 2; i++) {
        writer_ids[i] = i + 1;
        pthread_create(&writers[i], NULL, writer, &writer_ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }

    sem_destroy(&rmutex);
    sem_destroy(&wmutex);
    sem_destroy(&resource_access);
    sem_destroy(&read_try);

    return 0;
}
