# Custom Memory Allocator

A learning project implementing a custom `malloc` and `free` in C, built from scratch using `mmap` system calls. This allocator demonstrates fundamental memory management concepts including chunk-based allocation, coalescing, and efficient memory reuse strategies.

## Overview

This allocator manages memory through a linked list of heap chunks, each containing multiple memory blocks. It implements:

- **First-fit allocation strategy** with block splitting
- **Bidirectional coalescing** to reduce fragmentation
- **Bump allocation** for fast sequential allocations
- **Large allocation optimization** (dedicated chunks for allocations ≥1MB)

## Features

### Core Functionality

- `alloc_mem(size_t size)` - Allocate memory blocks
- `free_mem(void *ptr)` - Free allocated memory with automatic coalescing
- `alloc_heap_chunk(size_t size)` - Create new heap chunks as needed

### Key Design Decisions

**1. Embedded Metadata**
Each heap chunk's metadata is stored at the beginning of its `mmap`'d region, keeping everything self-contained:

```
[heap_chunk metadata][usable memory region...]
```

**2. Three-Tier Allocation Strategy**

- Try to reuse freed blocks (first-fit search)
- Try bump allocation from `curr_pos`
- Allocate new chunk as last resort

**3. Practical Chunk Sizing**

- Standard allocations: 1MB chunks
- Large allocations (≥1MB): Dedicated chunks sized to fit

**4. Automatic Coalescing**
When freeing memory, adjacent free blocks are automatically merged to reduce fragmentation:

```
Before: [used][free][free][free][used]
After:  [used][----free----][used]
```

**5. Contiguous Space Reclamation**
When the last block in a chunk is freed, it's reclaimed as contiguous space by moving `curr_pos` backwards, making it available for bump allocation.

## Data Structures

### HeapChunk

```c
typedef struct HeapChunk {
    void *head;              // Start of usable memory
    void *tail;              // End of usable memory
    void *curr_pos;          // Bump allocation pointer
    mem_header *first_block; // First allocated mem block
    mem_header *last_block;  // Last allocated mem block
    heap_chunk *prev_chunk;  // Previous heap chunk
    heap_chunk *next_chunk;  // Next heap chunk
    size_t total_size;       // Total usable bytes
    size_t free_size;        // Fragmented free space
    size_t free_cont_size;   // Contiguous free space
} heap_chunk;
```

### MemHeader

```c
typedef struct MemHeader {
    size_t size;          // Size of usable memory (excluding header)
    bool is_free;         // Allocation status
    heap_chunk *chunk;    // Parent heap chunk
    mem_header *prev_mem; // Previous mem block in list
    mem_header *next_mem; // Next mem block in list
} mem_header;
```

## Memory Layout Example

```
Chunk 1 (1MB):
┌──────────────┬────────────┬────────────┬────────────┬──────────────┐
│ heap_chunk   │ mem_header │ data (100) │ mem_header │ data (200)   │
│ metadata     │ [allocated]│            │ [free]     │              │
└──────────────┴────────────┴────────────┴────────────┴──────────────┘
                                                       ↑
                                                    curr_pos
```

## Performance Characteristics

| Operation               | Time Complexity | Notes                                 |
| ----------------------- | --------------- | ------------------------------------- |
| Allocation (best case)  | O(1)            | Bump allocation                       |
| Allocation (typical)    | O(n·m)          | n = heap chunks, m = memory blocks    |
| Allocation (worst case) | O(n·m)          | Must scan all chunks                  |
| Deallocation            | O(1) amortized  | Coalescing bounded by adjacent blocks |
| Memory overhead         | ~40 bytes       | Per allocation (mem_header)           |

## Building and Testing

### Compilation

```bash
gcc -Wall -Wextra -std=c11 -o allocator allocator.c
```

### Basic Usage Example

```c
#include "allocator.h"

int main(void) {
    // Allocate memory
    int *numbers = alloc_mem(sizeof(int) * 100);
    for (int i = 0; i < 100; i++) {
        numbers[i] = i;
    }

    // Free memory
    free_mem(numbers);

    return 0;
}
```

### Test Suite

```c
int main(void) {
    // Test 1: Basic allocation
    int *a = alloc_mem(sizeof(int));
    *a = 42;
    printf("Test 1: %d\n", *a);

    // Test 2: Multiple allocations
    int *arr = alloc_mem(sizeof(int) * 1000);
    for (int i = 0; i < 1000; i++) arr[i] = i;

    // Test 3: Free and reuse
    free_mem(a);
    int *b = alloc_mem(sizeof(int));
    *b = 99;

    // Test 4: Large allocation (gets dedicated chunk)
    char *big = alloc_mem(2 * 1024 * 1024);  // 2MB
    free_mem(big);

    // Test 5: Coalescing
    int *x = alloc_mem(100);
    int *y = alloc_mem(100);
    int *z = alloc_mem(100);
    free_mem(y);  // Create hole
    free_mem(x);  // Should coalesce with y
    free_mem(z);  // Should coalesce into one large block

    printf("All tests passed!\n");
    return 0;
}
```

## Known Limitations

### Current Issues

1. **No alignment guarantees** - May cause issues on architectures requiring aligned access
2. **No thread safety** - Not safe for multi-threaded use
3. **No chunk deallocation** - Empty chunks are never returned to the OS
4. **Linear search** - O(n·m) for finding free blocks in worst case
5. **No realloc** - Only malloc/free implemented

### Edge Cases

- Allocating 0 bytes returns NULL with error message
- Freeing NULL is allowed (no-op, matching standard `free`)
- Double-free is detected and produces error message
- Invalid pointers may cause undefined behavior

## Learning Outcomes

- How malloc/free work under the hood
- Memory fragmentation and coalescing strategies
- Pointer arithmetic and type casting in C
- System calls (`mmap`, `munmap`)
- Linked data structure design and manipulation
- Trade-offs between speed, memory overhead, and fragmentation

## License

This is a learning project - feel free to use, modify, and learn from it.

## Author

Created as a learning exercise to understand memory management at a low level. Not intended for production use.

---

**Note**: This allocator was built for educational purposes. For production code, use your system's `malloc` or established allocators like jemalloc or tcmalloc.
