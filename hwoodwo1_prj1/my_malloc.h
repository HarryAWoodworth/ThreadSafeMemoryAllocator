#ifndef __MY_MALLOC_H
#define __MY_MALLOC_H

#include <stdlib.h>
#include <pthread.h>

void my_malloc_init(size_t default_size, size_t num_arenas);

void my_malloc_destroy(void);

void* my_malloc(size_t size);

// Track a memory segment available for allocation: use a top pointer
typedef struct arena 
{
	// pointer to top
	void *top;
	// bump pointer
	void *bumpPtr;
	// remaining
	size_t remaining;
	// Next pointer
	struct arena *nextArena;
	// waitlock
	pthread_mutex_t waitlock;
} arena;

// Linked list of arenas
// Keep track of these in order to remove them in my_malloc_destroy
typedef struct arenaList
{
	// Pointer to the head of the list
	arena *head;
	// Pointer to the tail of the list
	arena *tail;
	// Size of the arenas
	size_t arena_size;
} arenaList;

typedef struct liveNode
{
	void *liveMemChunk;
	struct liveNode *next;
} liveNode;

typedef struct liveList
{
	// Pointer to head
	liveNode *head;
	// Pointer to tail
	liveNode *tail;
} liveList;

arenaList* new_arenaList(size_t arena_size_in);

void addArena(arenaList *al);

void free_arena_list(arenaList *al);

liveList* new_liveList();

void addLiveMemory(liveList *ll, void* liveMemChunkIn);

void free_liveList(liveList *ll);

#endif /* __MY_MALLOC_H */
