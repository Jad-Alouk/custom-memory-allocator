#include <stdio.h>
#include <stdbool.h>
#include <sys/mman.h>

#define HEAP_CHUNK_BASE_SIZE 1000000 // 1 MB

typedef struct HeapChunk heap_chunk;
typedef struct MemHeader mem_header;

typedef struct HeapChunk
{
    void *head;              // first ptr
    void *tail;              // last ptr
    void *curr_pos;          // all allocated mem blocks <-- curr_pos --> all unallocated mem blocks
    mem_header *first_block; // first allocated mem block
    mem_header *last_block;  // last allocated mem block
    heap_chunk *prev_chunk;  // prev heap chunk
    heap_chunk *next_chunk;  // next heap chunk
    size_t total_size;       // absolute size of the entire chunk
    size_t free_size;        // total size of all freed memory blocks in the chunk
    size_t free_cont_size;   // total space left after curr_pos
} heap_chunk;

typedef struct MemHeader
{
    size_t size;          // all bytes - sizeof(mem_header)
    bool is_free;         // for deallocation
    heap_chunk *chunk;    // heap chunk where a mem block belongs
    mem_header *prev_mem; // prev mem block within a heap chunk
    mem_header *next_mem; // next mem block within a heap chunk
} mem_header;

// Globals
heap_chunk *heap_head = NULL; // head of all heap chunks
heap_chunk *heap_tail = NULL; // tail of all heap chunks

heap_chunk *alloc_heap_chunk(size_t size);
void *alloc_mem(size_t size);
mem_header *find_mem_spot(size_t size);
void free_mem(void *ptr);

int main(void)
{
    return 0;
}

heap_chunk *alloc_heap_chunk(size_t chunk_size)
{
    heap_chunk *new_heap_chunk = mmap(
        NULL,
        sizeof(heap_chunk) + chunk_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0);

    if (new_heap_chunk == MAP_FAILED)
    {
        fprintf(stderr, "Failed to allocate memory for the heap chunk\n");
        return NULL;
    }

    new_heap_chunk->head = (void *)(new_heap_chunk + 1);
    new_heap_chunk->tail = (void *)((char *)(new_heap_chunk + 1) + chunk_size);
    new_heap_chunk->curr_pos = (void *)(new_heap_chunk + 1);
    new_heap_chunk->first_block = NULL;
    new_heap_chunk->last_block = NULL;
    new_heap_chunk->next_chunk = NULL;
    new_heap_chunk->total_size = chunk_size;
    new_heap_chunk->free_size = 0;
    new_heap_chunk->free_cont_size = chunk_size;

    if (heap_head == NULL)
    {
        new_heap_chunk->prev_chunk = NULL;
        heap_head = new_heap_chunk;
        heap_tail = new_heap_chunk;
    }

    else
    {
        heap_tail->next_chunk = new_heap_chunk;
        new_heap_chunk->prev_chunk = heap_tail;
        heap_tail = new_heap_chunk;
    }

    return new_heap_chunk;
}

void *alloc_mem(size_t size)
{
    if (size == 0)
    {
        fprintf(stderr, "Allocating zero bytes is not allowed\n");
        return NULL;
    }

    size_t total_mem_size = sizeof(mem_header) + size;
    heap_chunk *curr_heap_chunk = NULL;

    if (total_mem_size >= HEAP_CHUNK_BASE_SIZE)
    {
        curr_heap_chunk = alloc_heap_chunk(total_mem_size);

        if (curr_heap_chunk == NULL)
        {
            return NULL;
        }

        mem_header *mem = curr_heap_chunk->head;
        mem->size = size;
        mem->is_free = false;
        mem->chunk = curr_heap_chunk;
        mem->prev_mem = NULL;
        mem->next_mem = NULL;

        curr_heap_chunk->curr_pos = (void *)((char *)curr_heap_chunk->head + total_mem_size);
        curr_heap_chunk->first_block = mem;
        curr_heap_chunk->last_block = mem;
        curr_heap_chunk->free_size = 0;
        curr_heap_chunk->free_cont_size = 0;

        return (void *)(mem + 1);
    }

    mem_header *mem_spot = find_mem_spot(size);

    if (mem_spot == NULL)
    {
        curr_heap_chunk = alloc_heap_chunk(HEAP_CHUNK_BASE_SIZE);

        if (curr_heap_chunk == NULL)
        {
            return NULL;
        }

        mem_header *mem = curr_heap_chunk->head;
        mem->size = size;
        mem->is_free = false;
        mem->chunk = curr_heap_chunk;
        mem->prev_mem = NULL;
        mem->next_mem = NULL;

        curr_heap_chunk->curr_pos = (void *)((char *)curr_heap_chunk->head + total_mem_size);
        curr_heap_chunk->first_block = mem;
        curr_heap_chunk->last_block = mem;
        curr_heap_chunk->free_size = 0;
        curr_heap_chunk->free_cont_size = curr_heap_chunk->total_size - total_mem_size;

        return (void *)(mem + 1);
    }

    return (void *)(mem_spot + 1);
}

mem_header *find_mem_spot(size_t size)
{
    if (heap_head == NULL)
    {
        return NULL;
    }

    heap_chunk *curr_heap_chunk = heap_head;

    while (curr_heap_chunk != NULL)
    {
        // search for a free mem block
        if (curr_heap_chunk->free_size >= size)
        {
            mem_header *curr_mem = curr_heap_chunk->head;

            while (curr_mem != NULL)
            {
                if (curr_mem->is_free && curr_mem->size >= size)
                {
                    size_t mem_diff = curr_mem->size - size;

                    curr_mem->is_free = false;
                    curr_mem->size = size;

                    if (mem_diff >= sizeof(mem_header) + 1)
                    {
                        mem_header *new_mem = (void *)((char *)(curr_mem + 1) + size);
                        new_mem->size = mem_diff - sizeof(mem_header);
                        new_mem->is_free = true;
                        new_mem->chunk = curr_heap_chunk;
                        new_mem->next_mem = curr_mem->next_mem;
                        new_mem->prev_mem = curr_mem;
                        curr_mem->next_mem = new_mem;

                        if (new_mem->next_mem != NULL)
                        {
                            new_mem->next_mem->prev_mem = new_mem;
                        }

                        else
                        {
                            curr_heap_chunk->last_block = new_mem;
                        }

                        curr_heap_chunk->free_size -= (sizeof(mem_header) + size);
                    }

                    else
                    {
                        curr_mem->size += mem_diff;
                        curr_heap_chunk->free_size -= curr_mem->size;
                    }

                    return curr_mem;
                }

                curr_mem = curr_mem->next_mem;
            }
        }

        // linear bump allocation
        if (curr_heap_chunk->free_cont_size >= sizeof(mem_header) + size)
        {
            mem_header *mem = curr_heap_chunk->curr_pos;
            mem->size = size;
            mem->is_free = false;
            mem->chunk = curr_heap_chunk;
            mem->prev_mem = curr_heap_chunk->last_block;
            mem->next_mem = NULL;

            if (curr_heap_chunk->last_block != NULL)
            {
                curr_heap_chunk->last_block->next_mem = mem;
            }

            if (curr_heap_chunk->first_block == NULL)
            {
                curr_heap_chunk->first_block = mem;
            }

            curr_heap_chunk->curr_pos = (void *)((char *)curr_heap_chunk->curr_pos + sizeof(mem_header) + size);
            curr_heap_chunk->last_block = mem;
            curr_heap_chunk->free_cont_size -= (sizeof(mem_header) + size);

            return mem;
        }

        curr_heap_chunk = curr_heap_chunk->next_chunk;
    }

    return NULL;
}

void free_mem(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    if (heap_head == NULL)
    {
        fprintf(stderr, "Heap is not initialized\n");
        return;
    }

    mem_header *mem = (mem_header *)ptr - 1;

    if (mem->is_free)
    {
        fprintf(stderr, "Double free is not allowed\n");
        return;
    }

    heap_chunk *chunk = mem->chunk;

    mem->is_free = true;
    chunk->free_size += mem->size;

    while (mem->prev_mem != NULL && mem->prev_mem->is_free)
    {
        mem_header *prev = mem->prev_mem;

        prev->size += sizeof(mem_header) + mem->size;
        prev->next_mem = mem->next_mem;

        if (mem->next_mem != NULL)
        {
            mem->next_mem->prev_mem = prev;
        }

        else
        {
            chunk->last_block = prev;
        }

        chunk->free_size += sizeof(mem_header);
        mem = prev;
    }

    while (mem->next_mem != NULL && mem->next_mem->is_free)
    {
        mem_header *next = mem->next_mem;

        mem->size += sizeof(mem_header) + next->size;
        mem->next_mem = next->next_mem;

        if (next->next_mem != NULL)
        {
            next->next_mem->prev_mem = mem;
        }

        else
        {
            chunk->last_block = mem;
        }

        chunk->free_size += sizeof(mem_header);
    }

    // if this is the last allocated block, we can reclaim as contiguous space
    if (mem->next_mem == NULL)
    {
        chunk->curr_pos = (void *)mem;
        chunk->free_cont_size += sizeof(mem_header) + mem->size;
        chunk->free_size -= mem->size;

        chunk->last_block = mem->prev_mem;

        if (mem->prev_mem != NULL)
        {
            mem->prev_mem->next_mem = NULL;
        }
        else
        {
            chunk->first_block = NULL;
        }
    }
}
