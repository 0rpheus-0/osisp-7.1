#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

struct Buffer
{
    int added;
    int extracted;
    int capacity;
    int begin;
    int end;
    char data[];
};

void *smalloc(int size)
{
    void *block = mmap(
        NULL,
        size + sizeof(size),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0);
    *((int *)block) = size;
    return (char *)block + sizeof(size);
}

void sfree(void *shared)
{
    void *block = (char *)shared - sizeof(int);
    munmap(block, *((int *)block));
}

static struct Buffer *createBuffer(struct Buffer *buffer, int size)
{
    *buffer = (struct Buffer){
        .added = 0,
        .extracted = 0,
        .capacity = size,
        .begin = 0,
        .end = 0};
    return buffer;
}

void freeDesctruct(struct Buffer *buffer)
{
    sfree(buffer);
}

int length(struct Buffer *buffer)
{
    return buffer->begin <= buffer->end
               ? buffer->end - buffer->begin
               : ((buffer->end - 0) + (buffer->capacity - buffer->begin));
}

int availableBuffer(struct Buffer *buffer)
{
    return buffer->capacity - 1 - length(buffer);
}

int allocBuffer(struct Buffer *buffer, int size)
{
    if (size < 0)
        return -1;
    if (availableBuffer(buffer) < size)
        return -1;
    buffer->end = (buffer->end + size) % buffer->capacity;
    return 0;
}

int freeBuffer(struct Buffer *buffer, int size)
{
    if (size < 0)
        return -1;
    if (length(buffer) < size)
        return -1;
    buffer->begin = (buffer->begin + size) % buffer->capacity;
    return 0;
}

char *bufferByte(struct Buffer *buffer, int index)
{
    return &(buffer->data[index % buffer->capacity]);
}

int sendBytes(struct Buffer *buffer, int count, char bytes[])
{
    if (count >= buffer->capacity)
        return -1;

    int base = buffer->end;
    while (allocBuffer(buffer, count) == -1)
        ;
    for (int i = 0; i < count; i++)
    {
        *bufferByte(buffer, base + i) = bytes[i];
    }
    return 0;
}

int readBytes(struct Buffer *buffer, int count, uint8_t bytes[])
{
    int base = buffer->begin;
    while (length(buffer) < count)
        ;
    for (int i = 0; i < count; i++)
    {
        bytes[i] = *bufferByte(buffer, base + i);
    }
    freeBuffer(buffer, count);
    return 0;
}
