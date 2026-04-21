#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_ROVERS  5
#define NUM_CABLES  5

/* ── Battery constants ─────────────────────────────── */
#define BATTERY_INIT          50.0
#define BATTERY_CRITICAL      20.0   /* Emergency threshold          */
#define BATTERY_DEAD           0.0
#define BATTERY_DRAIN_EXPLORE 10.0   /* % per second while exploring */
#define BATTERY_DRAIN_WAIT     5.0   /* % per second while waiting   */
#define BATTERY_CHARGE_RATE   15.0   /* % per second while charging  */

/* ── Monitor state ─────────────────────────────────── */
static int cable_free[NUM_CABLES];          /* 1 = free, 0 = in use  */
static pthread_mutex_t  hub_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   hub_cond    = PTHREAD_COND_INITIALIZER;

/* ── Per-rover data ─────────────────────────────────── */
typedef struct {
    int    id;
    double battery;
    int    emergency;   /* 1 if battery < CRITICAL */
    int    stranded;    /* 1 if battery reached 0  */
} Rover;

static Rover rovers[NUM_ROVERS];

/* ──────────────────────────────────────────────────── */
/*  Helper: sleep for 'seconds' while draining battery */
/*  Returns 1 if rover died during sleep, else 0       */
static int sleep_drain(Rover *r, double seconds, double rate_per_sec)
{
    /* simulate in 1-second ticks */
    int ticks = (int)(seconds);
    for (int i = 0; i < ticks; i++) {
        usleep(1000000);   /* 1 s */
        r->battery -= rate_per_sec * 1.0;
        if (r->battery <= BATTERY_DEAD) {
            r->battery = BATTERY_DEAD;
            return 1;   /* dead */
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────── */
/*  Try to acquire both cables (left = id, right = (id+1)%N) */
/*  Emergency rovers are served before normal rovers.  */
static void acquire_cables(Rover *r)
{
    int left  = r->id;
    int right = (r->id + 1) % NUM_CABLES;

    pthread_mutex_lock(&hub_mutex);

    while (1) {
        /* Check emergency flag */
        if (r->battery < BATTERY_CRITICAL && !r->emergency) {
            r->emergency = 1;
            printf("Rover %d ENTERING EMERGENCY STATE (Battery: %.0f%%)\n",
                   r->id, r->battery);
        }

        /* A normal rover must yield if ANY other rover is in emergency */
        int other_emergency = 0;
        if (!r->emergency) {
            for (int i = 0; i < NUM_ROVERS; i++) {
                if (i != r->id && rovers[i].emergency && !rovers[i].stranded) {
                    other_emergency = 1;
                    break;
                }
            }
        }

        if (!other_emergency && cable_free[left] && cable_free[right]) {
            /* Grab both cables atomically inside the lock */
            cable_free[left]  = 0;
            cable_free[right] = 0;
            break;
        }

        /* Drain battery while waiting */
        pthread_mutex_unlock(&hub_mutex);
        r->battery -= BATTERY_DRAIN_WAIT * 1.0;
        if (r->battery <= BATTERY_DEAD) {
            r->battery  = BATTERY_DEAD;
            r->stranded = 1;
            printf("Rover %d STRANDED: Battery depleted to 0%% while waiting to charge.\n",
                   r->id);
            pthread_cond_broadcast(&hub_cond);
            pthread_exit(NULL);
        }
        usleep(1000000);   /* 1 s tick */

        pthread_mutex_lock(&hub_mutex);

        /* Re-evaluate emergency after drain */
        if (r->battery < BATTERY_CRITICAL && !r->emergency) {
            r->emergency = 1;
            printf("Rover %d ENTERING EMERGENCY STATE (Battery: %.0f%%)\n",
                   r->id, r->battery);
        }
        /* Wake others in case conditions changed */
        pthread_cond_broadcast(&hub_cond);
    }

    pthread_mutex_unlock(&hub_mutex);
}

/* ──────────────────────────────────────────────────── */
static void release_cables(Rover *r)
{
    int left  = r->id;
    int right = (r->id + 1) % NUM_CABLES;

    pthread_mutex_lock(&hub_mutex);
    cable_free[left]  = 1;
    cable_free[right] = 1;
    r->emergency      = 0;   /* reset after a successful charge */
    pthread_cond_broadcast(&hub_cond);
    pthread_mutex_unlock(&hub_mutex);
}

/* ──────────────────────────────────────────────────── */
/*  Rover thread main loop                             */
static void *rover_thread(void *arg)
{
    Rover *r = (Rover *)arg;
    srand((unsigned)(time(NULL) ^ (r->id * 12345)));

    while (1) {
        /* ── EXPLORING ────────────────────────────── */
        int explore_secs = 1 + rand() % 5;   /* 1–5 s */
        printf("Rover %d is exploring (Battery: %.0f%%)\n",
               r->id, r->battery);

        int died = sleep_drain(r, explore_secs, BATTERY_DRAIN_EXPLORE);
        if (died) {
            printf("Rover %d STRANDED: Battery depleted to 0%% during exploration.\n",
                   r->id);
            pthread_mutex_lock(&hub_mutex);
            r->stranded = 1;
            pthread_cond_broadcast(&hub_cond);
            pthread_mutex_unlock(&hub_mutex);
            pthread_exit(NULL);
        }

        /* Check emergency BEFORE waiting */
        if (r->battery < BATTERY_CRITICAL) {
            pthread_mutex_lock(&hub_mutex);
            if (!r->emergency) {
                r->emergency = 1;
                printf("Rover %d ENTERING EMERGENCY STATE (Battery: %.0f%%)\n",
                       r->id, r->battery);
            }
            pthread_mutex_unlock(&hub_mutex);
        }

        /* ── WAITING FOR CABLES ────────────────────── */
        printf("Rover %d is waiting for cables (Battery: %.0f%%)\n",
               r->id, r->battery);

        acquire_cables(r);   /* blocks + drains until cables acquired */

        /* ── CHARGING ─────────────────────────────── */
        printf("Rover %d is charging (Battery: %.0f%%)\n",
               r->id, r->battery);

        int charge_secs = 1 + rand() % 3;    /* 1–3 s */
        for (int s = 0; s < charge_secs; s++) {
            usleep(1000000);  /* 1 second */
            r->battery += BATTERY_CHARGE_RATE;
            if (r->battery > 100.0) r->battery = 100.0;
        }

        release_cables(r);

        printf("Rover %d finished charging (Battery: %.0f%%)\n",
               r->id, r->battery);
    }

    return NULL;
}

/* ──────────────────────────────────────────────────── */
int main(void)
{
    pthread_t threads[NUM_ROVERS];

    /* Init cables */
    for (int i = 0; i < NUM_CABLES; i++)
        cable_free[i] = 1;

    /* Init rovers */
    for (int i = 0; i < NUM_ROVERS; i++) {
        rovers[i].id        = i;
        rovers[i].battery   = BATTERY_INIT;
        rovers[i].emergency = 0;
        rovers[i].stranded  = 0;
    }

    /* Spawn rover threads */
    for (int i = 0; i < NUM_ROVERS; i++)
        pthread_create(&threads[i], NULL, rover_thread, &rovers[i]);

    /* Join threads (only returns when all rovers strand) */
    for (int i = 0; i < NUM_ROVERS; i++)
        pthread_join(threads[i], NULL);

    printf("All rovers have been stranded. Simulation complete.\n");
    return 0;
}
