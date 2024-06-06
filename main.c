#define _GNU_SOURCE

#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <time.h>

int items = 0;
int freeSpase = 5;

pthread_mutex_t mutex;
pthread_cond_t cond_read;
pthread_cond_t cond_sent;

struct Buffer *buffer;

pthread_t *consumes = NULL;
int *consumeRun = NULL;
int consumeCount = 0;

pthread_t *produces = NULL;
int *produceRun = NULL;
int produceCount = 0;

struct Message
{
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    char data[];
};

#define MESSAGE_MAX_SIZE (sizeof(struct Message) + 255)

int MessageSize(struct Message *message) { return sizeof(*message) + message->size; }

struct Message *readMessage(struct Buffer *buffer)
{
    struct Message *head = alloca(MESSAGE_MAX_SIZE);
    readBytes(buffer, sizeof(struct Message), (char *)head);
    readBytes(buffer, head->size, head->data);
    buffer->extracted++;
    printf("Extracted : %d\n", buffer->extracted);
    return memcpy(malloc(MessageSize(head)), head, MessageSize(head));
}

void sendMessage(struct Buffer *buffer, struct Message *message)
{
    sendBytes(buffer, MessageSize(message), (char *)message);
    buffer->added++;
    printf("Added : %d\n", buffer->added);
}

uint16_t xor (int length, char bytes[]) {
    uint16_t res = 0;
    for (int i = 0; i < length; i++)
    {
        res ^= bytes[i];
    }
    return res;
}

    struct Message *randomMessage()
{
    uint8_t size = rand() % 256;
    struct Message *message = malloc(sizeof(struct Message) + size);
    *message = (struct Message){
        .type = rand(),
        .hash = 0,
        .size = size,
    };
    for (int i = 0; i < size; i++)
        message->data[i] = rand();
    message->hash = xor(sizeof(struct Message) + size, (char *)message);
    return message;
}

void produce(void *arg)
{
    while ((*(int *)arg))
    {
        sleep(1);
        pthread_mutex_lock(&mutex);
        while (freeSpase <= 0)
            pthread_cond_wait(&cond_sent, &mutex);
        freeSpase--;
        struct Message *message = randomMessage();
        sendMessage(buffer, message);
        printf(
            "Producer %5lu Sent message with type %02hX and hash %04hX\n",
            pthread_self(),
            message->type,
            message->hash);
        free(message);
        items++;
        pthread_cond_broadcast(&cond_read);
        pthread_mutex_unlock(&mutex);
        pthread_testcancel();
    }
}

void consume(void *arg)
{
    while ((*(int *)arg))
    {
        sleep(1);
        pthread_mutex_lock(&mutex);
        while (items <= 0)
            pthread_cond_wait(&cond_read, &mutex);
        items--;
        struct Message *message = readMessage(buffer);
        printf(
            "Consumer %5lu Got  message with type %02hX and hash %04hX\n",
            pthread_self(),
            message->type,
            message->hash);
        free(message);
        freeSpase++;
        pthread_cond_broadcast(&cond_sent);
        pthread_mutex_unlock(&mutex);
        pthread_testcancel();
    }
}

void newProduce()
{
    produceCount++;
    produces = realloc(produces, produceCount * sizeof(pthread_t));
    produceRun = realloc(produceRun, produceCount * sizeof(int));
    produceRun[produceCount - 1] = 1;
    pthread_create(&produces[produceCount - 1], NULL, produce, &produceRun[produceCount - 1]);
    printf("New produce: %lu\n", produces[produceCount - 1]);
}

void killProduce()
{
    if (produceCount == 0)
        return;
    produceCount--;
    produceRun[produceCount] = 0;
    printf("Kill Produce TID: %lu\n", produces[produceCount]);
    items--;
    freeSpase++;
    pthread_cond_broadcast(&cond_sent);
    pthread_cancel(produces[produceCount]);
    pthread_join(produces[produceCount], NULL);
}

void killAllProduce()
{

    while (produceCount)
        killProduce();
    free(produces);
    produces = NULL;
}

void newConsume()
{
    consumeCount++;
    consumes = realloc(consumes, consumeCount * sizeof(pthread_t));
    consumeRun = realloc(consumeRun, consumeCount * sizeof(int));
    consumeRun[consumeCount - 1] = 1;
    pthread_create(&consumes[consumeCount - 1], NULL, consume, &consumeRun[consumeCount - 1]);
    printf("New consume: %lu\n", consumes[consumeCount - 1]);
}

void killConsume()
{
    if (consumeCount == 0)
        return;
    consumeCount--;
    consumeRun[consumeCount] = 0;
    printf("Kill Consume TID: %lu\n", consumes[consumeCount]);
    freeSpase--;
    items++;
    pthread_cond_broadcast(&cond_read);
    pthread_cancel(consumes[consumeCount]);
    pthread_join(consumes[consumeCount], NULL);
}

void killAllConsume()
{
    while (consumeCount)
        killConsume();
    free(consumes);
    consumes = NULL;
}

int main()
{
    int queue = 5;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_sent, NULL);
    pthread_cond_init(&cond_read, NULL);
    //-----------------------------------------------//
    int capacity = 1024;
    buffer = createBuffer(smalloc(sizeof(struct Buffer) + capacity), capacity);
    char opt[256];
    printf("Start program\n");
    while (scanf("%s", opt))
    {
        if (!strcmp(opt, "p"))
            newProduce(buffer);
        if (!strcmp(opt, "kp"))
            killProduce();
        if (!strcmp(opt, "kap"))
            killAllProduce();
        if (!strcmp(opt, "c"))
            newConsume(buffer);
        if (!strcmp(opt, "kc"))
            killConsume();
        if (!strcmp(opt, "kac"))
            killAllConsume();
        if (!strcmp(opt, "ka"))
        {
            killAllConsume();
            killAllProduce();
        }
        if (!strcmp(opt, "q"))
            break;
    }
    //-----------------------------------------------//
    killAllConsume();
    killAllProduce();
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_sent);
    pthread_cond_destroy(&cond_read);
    freeDesctruct(buffer);
    return 0;
}
