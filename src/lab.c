#include "../src/lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>


size_t btok(size_t bytes){
    unsigned int count = 0;
    bytes--;
    while (bytes > 0) {bytes = bytes >> 1; count++;} //bytes >>= 1
    return count;
}

struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy){
    size_t block_size = 1 << buddy->kval;  // Size of the block in bytes
    size_t offset = (size_t)(buddy) - (size_t)(pool->base);  // Offset from base address
    size_t buddy_offset = offset ^ block_size;  // XOR operation to find buddy

    return (struct avail*)((char*)pool->base + buddy_offset);  // Return the buddy block address
}

void *buddy_malloc(struct buddy_pool *pool, size_t size){
    if (!pool || size == 0) {
        return NULL;  // Invalid input
    }

    size_t kval_needed = btok(size);  // Find the K value for the requested size
    if (kval_needed < SMALLEST_K) {
        kval_needed = SMALLEST_K;  // Ensure we don't allocate smaller than the minimum size
    }

    // Search for the smallest available block
    struct avail *block = NULL;
    for (size_t kval = kval_needed; kval <= pool->kval_m; kval++) {
        if (pool->avail[kval].tag == BLOCK_AVAIL) {
            block = &pool->avail[kval];  // Found a suitable block
            break;
        }
    }

    if (!block) {
        return NULL;  // No available block large enough
    }

    // Split blocks if necessary to match the requested size
    while (block->kval > kval_needed) {
        struct avail *buddy = buddy_calc(pool, block);  // Find the buddy block
        block->kval--;  // Split the block into two
        buddy->kval = block->kval;  // Set buddy's K value to match

        // Mark the buddy as available
        buddy->tag = BLOCK_AVAIL;
        buddy->next = pool->avail[block->kval].next;
        buddy->prev = pool->avail[block->kval].prev;

        block = buddy;  // Move to the buddy block
    }

    // Mark the allocated block as reserved
    block->tag = BLOCK_RESERVED;
    return (void*)block;  // Return the address of the allocated block
}

void buddy_free(struct buddy_pool *pool, void *ptr){
    if (!ptr || !pool) {
        return;  // Invalid input
    }

    struct avail *block = (struct avail*)ptr;
    block->tag = BLOCK_AVAIL;  // Mark the block as available

    // Try to merge the block with its buddy if both are free
    struct avail *buddy = buddy_calc(pool, block);
    if (buddy->tag == BLOCK_AVAIL) {
        // Merge the buddies
        if (block < buddy) {
            block->kval++;
        } else {
            buddy->kval++;
        }
        buddy->tag = BLOCK_UNUSED;  // Mark buddy as unused
    }
}

void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size){
    if (!ptr) {
        return buddy_malloc(pool, size);  // If ptr is NULL, malloc the memory
    }
    if (size == 0) {
        buddy_free(pool, ptr);  // If size is zero, free the memory
        return NULL;
    }

    // Get the current block and its size
    struct avail *block = (struct avail*)ptr;
    size_t current_size = 1 << block->kval;

    if (size <= current_size) {
        return ptr;  // No need to realloc if the requested size is smaller or equal
    }

    // Allocate a new larger block and copy the old data to the new block
    void *new_block = buddy_malloc(pool, size);
    if (new_block) {
        memcpy(new_block, ptr, current_size);  // Copy old data to new block
        buddy_free(pool, ptr);  // Free the old block
    }

    return new_block;  // Return the newly allocated block
}

void buddy_init(struct buddy_pool *pool, size_t size){

    if(size == 0){
        size = UINT64_C(1) << DEFAULT_K;
    }
    pool->kval_m = btok(size);
    pool->numbytes = UINT64_C(1) << pool->kval_m;

    pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(pool->base == MAP_FAILED){
        perror("buddy: could not allocate memory pool!");
    }

    for(int i = 0; i < pool->kval_m; i++){
        pool->avail[i].next = &pool->avail[i];
        pool->avail[i].prev = &pool->avail[i];
        pool->avail[i].kval = i;
        pool->avail[i].tag = BLOCK_UNUSED;
    }

    pool->avail[pool->kval_m].next = pool->base;
    pool->avail[pool->kval_m].prev = pool->base;

    struct avail *ptr = (struct avail *) pool->base;
    ptr->tag = BLOCK_AVAIL;
    ptr->kval = pool->kval_m;
    ptr->next = &pool->avail[pool->kval_m];
    ptr->prev = &pool->avail[pool->kval_m];

}

void buddy_destroy(struct buddy_pool *pool){
    int status = munmap(pool->base, pool->numbytes);
    if(status == -1){
        perror("buddy: destroy failed!");
    }
}

int myMain(int argc, char** argv);
