#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

// initial params
#define max_patients 50
#define chairs 5

// states
#define IDLE 0
#define WAITING 1
#define TREATING 2
#define FINISH 3

struct Patient {
    int id;
    int arrival;
    int visits;
    int timeouts;
    int turnaways;
    int treated;
};

int n, x, y, k;
int arrivals[max_patients];
int waiting_patients = 0;
int active_patients = 0;
int state[max_patients];

struct Patient p[max_patients];

// initialize mutex and semaphores
pthread_mutex_t mutex;
sem_t dentist_sem, patient_sem[max_patients];

// dentist and patient threads
void* patient(void* arg) {
    int i = *((int*)arg);
    free(arg);

    sleep(p[i].arrival);

    while (p[i].visits < x && !p[i].treated) {
        p[i].visits++;
 
        while (sem_trywait(&patient_sem[i]) == 0);

        pthread_mutex_lock(&mutex);

        // chairs full
        if (waiting_patients == chairs) {
            p[i].turnaways++;
            pthread_mutex_unlock(&mutex);
            sleep(y);
            continue;
        }

        // chair free
        state[i] = WAITING;
        waiting_patients++;
        pthread_mutex_unlock(&mutex);

        if (waiting_patients == 1) sem_post(&dentist_sem);

        struct timespec timesp;
        clock_gettime(CLOCK_REALTIME, &timesp);
        timesp.tv_sec += k;

        if (sem_timedwait(&patient_sem[i], &timesp) == 0) {
            pthread_mutex_lock(&mutex);
            p[i].treated = 1;
            state[i] = FINISH;
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (errno == ETIMEDOUT) {
            pthread_mutex_lock(&mutex);
            if (state[i] == WAITING) {
                p[i].timeouts++;
                state[i] = IDLE;
                waiting_patients--;
                pthread_mutex_unlock(&mutex);
                sleep(y);
                continue;
            }
            else if (state[i] == TREATING) {
                p[i].treated = 1;
                pthread_mutex_unlock(&mutex);
                sem_wait(&patient_sem[i]);
                break;
            }

            pthread_mutex_unlock(&mutex);
        }
    }

    pthread_mutex_lock(&mutex);

    active_patients--;

    // exit if no active patients
    if (active_patients == 0) sem_post(&dentist_sem);

    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* dentist(void* arg) {
    // runs until all patients treated or left
    while (true) {
        pthread_mutex_lock(&mutex);
        
        if (active_patients == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);

        sem_wait(&dentist_sem);

        while (true) {
            pthread_mutex_lock(&mutex);

            int f = -1;
            for (int i = 0; i < n; i++) {
                if (state[i] == WAITING) {
                    f = i;
                    state[i] = TREATING;
                    waiting_patients--;
                    break;
                }
            }

            pthread_mutex_unlock(&mutex);

            if (f == -1) break;

            sem_post(&patient_sem[f]);
            sleep(1);
        }
    }
    return NULL;
}


int main() {
    FILE *fileptr = fopen("input.txt", "r");

    // patients, wait time, num visits to give up, return time
    fscanf(fileptr, "%d %d %d %d", &n, &k, &x, &y);

    for (int i = 0; i < n; i++) {
        fscanf(fileptr, "%d", &arrivals[i]);
    }
    fclose(fileptr);

    for (int i = 0; i < n; i++) {
        p[i].id = i;
        p[i].arrival = arrivals[i];
        p[i].visits = 0;
        p[i].timeouts = 0;
        p[i].turnaways = 0;
        p[i].treated = 0;
        state[i] = IDLE;
        sem_init(&patient_sem[i], 0, 0);
    }

    pthread_mutex_init(&mutex, NULL);
    sem_init(&dentist_sem, 0, 0);
    active_patients = n;
    pthread_t dentist_thread;
    pthread_t patient_threads[max_patients];

    pthread_create(&dentist_thread, NULL, dentist, NULL);

    for (int i = 0; i < n; i++) {
        int* arg = malloc(sizeof(*arg));
        *arg = i;
        pthread_create(&patient_threads[i], NULL, patient, arg);
    }

    for (int i = 0; i < n; i++) pthread_join(patient_threads[i], NULL);
    pthread_join(dentist_thread, NULL);

    // FINAL OUTPUT
    printf("---CLINIC REPORT---\n");
    
    int treated = 0;
    for (int i = 0; i < n; i++) {
        printf("Patient %d- Status: %s | Visits: %d | Timeouts: %d | Turnaways: %d\n",
        i+1, (state[i] == FINISH ? "TREATED" : "UNTREATED"), p[i].visits, p[i].timeouts, p[i].turnaways);
        if (state[i] == FINISH) treated++;
    }

    printf("Total treated: %d\n", treated);
    printf("Total untreated: %d\n", n - treated);

    // free
    pthread_mutex_destroy(&mutex);
    sem_destroy(&dentist_sem);
    for (int i = 0; i < n; i++) sem_destroy(&patient_sem[i]);

    return 0;
}
