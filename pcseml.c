#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "eventbuf.h"
#include <pthread.h>
#include <semaphore.h>

int producer_count;
int consumer_count;
int event_count;
int max_buffer_size;
struct eventbuf *event_buffer;
sem_t *mutex;
sem_t *items;
sem_t *spaces;

sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

void *run_producer(void *arg)
{
    int *id = arg;
    
    for(int i = 0; i < event_count; i++)
    {   
        int event_num = *id * 100 + i;
        sem_wait(spaces);
        sem_wait(mutex);
        printf("P%d: adding event %d\n", *id, event_num);
        eventbuf_add(event_buffer, event_num);
        sem_post(mutex);
        sem_post(items);
    }
    printf("P%d: exiting\n", *id);
    return NULL;
}

void *run_consumer(void *arg)
{
    int *id = arg;
    while(1)
    {
        sem_wait(items);
        sem_wait(mutex);
        if(eventbuf_empty(event_buffer))
        {
            sem_post(mutex);
            break;
        }
        int event_num = eventbuf_get(event_buffer);
        printf("C%d: got event %d\n", *id, event_num);
        sem_post(mutex);
        sem_post(spaces);
    }
    printf("C%d: exiting\n", *id);
    return NULL;
}

int main(int argc, char *argv[]) 
{
    if (argc != 5) {
        fprintf(stderr, "usage: pcseml producer_count consumer_count event_count max_buffer_size \n");
        exit(1);
    }

    producer_count = atoi(argv[1]);
    consumer_count = atoi(argv[2]);
    event_count = atoi(argv[3]);
    max_buffer_size = atoi(argv[4]);

    event_buffer = eventbuf_create();
    mutex = sem_open_temp("event-buffer-mutex", 1);
    items = sem_open_temp("event-buffer-items", 0);
    spaces = sem_open_temp("event-buffer-spaces", max_buffer_size);

    pthread_t *prod_thread = calloc(producer_count, sizeof *prod_thread);
    int *prod_thread_id = calloc(producer_count, sizeof *prod_thread_id);
    pthread_t *cons_thread = calloc(consumer_count , sizeof *cons_thread);
    int *cons_thread_id = calloc(consumer_count, sizeof *cons_thread_id);

    for (int i = 0; i < producer_count; i++) {
        prod_thread_id[i] = i;
        pthread_create(prod_thread + i, NULL, run_producer, &prod_thread_id[i]);
    }

    for (int i = 0; i < consumer_count; i++){
        cons_thread_id[i] = i;
        pthread_create(cons_thread + i, NULL, run_consumer,  &cons_thread_id[i]);
    }

    for (int i = 0; i < producer_count; i++){
        pthread_join(prod_thread[i], NULL);
    }
    
    for (int i = 0; i < consumer_count ; i++){
        sem_post(items);
    }
    
    for (int i = 0; i < consumer_count ; i++){
        pthread_join(cons_thread[i], NULL);
    }
    eventbuf_free(event_buffer);
}
