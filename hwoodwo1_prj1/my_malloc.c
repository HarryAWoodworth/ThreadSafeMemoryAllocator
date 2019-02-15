// HARRY WOODWORTH
// CS476 Project 1 - Thread safe memory allocator

#include "my_malloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

// Create a new live memory list
liveList* new_liveList()
{
	liveList* ll = malloc(sizeof(liveList));
	ll->head = NULL;
	ll->tail = NULL;
	return ll;
}

// Take in a pointer to live memory and add a liveNode to the liveList
void addLiveMemory(liveList *ll, void* liveMemChunkIn)
{
	liveNode *n = malloc(sizeof(liveNode));
	n->liveMemChunk = liveMemChunkIn;
	n->next = NULL;

	if(ll->head == NULL) { ll->head = n; }
	if(ll->tail == NULL) {
		ll->tail = n;
	} else {
		ll->tail->next = n;
		ll->tail = n;
	}
}

// Free the live memory chunks in the list and free the nodes and list itself
void free_liveList(liveList *ll)
{
	liveNode *n = ll->head;
	while(n != NULL) 
	{
		liveNode *tempNode = n;
		n = n->next;
		// Free the live memory chunk
		free(tempNode->liveMemChunk);
		// Free the node
		free(tempNode);
	}
	// Free the lsit
	free(ll);
}

// Create a new arenaList, takes in the size of an arena
arenaList* new_arenaList(size_t arena_size_in)
{
	// Allocate memory for the arena list
	arenaList *al = malloc(sizeof(arenaList));
	// Set the head and tail to null
	al->head = NULL;
	al->tail = NULL;
	// Set the arena_size
	al->arena_size = arena_size_in;
	// Return the arena
	return al;
}

// Instantiate an arena and add it to the linked list
void addArena(arenaList *al)
{
	// Allocate memory for arena
	arena *a = (arena*)malloc(sizeof(arena));
	// Allocate a chunk of memory based on arena_size at top pointer
	a->top = malloc(al->arena_size);
	// Set the bump pointer to the start of the chunk
	a->bumpPtr = a->top;
	// Set size remaining to arena_size
	a->remaining = al->arena_size;
	// Set the nextArena to null
	a->nextArena = NULL;

	// If the list is empty add it to the head
	if(al->head == NULL) { al->head = a; }

	// Set it to the tail if the list is empty
	if(al->tail == NULL) { 
		al->tail = a; 
	// Else add the arena to the end of the list and set tail to it
	} else {
		al->tail->nextArena = a;
		al->tail = a;
	}
}

// Free each arena in the arena list
void free_arena_list(arenaList *al)
{
	// Get the head of the lsit
	arena *a = al->head;
	// Recursively free the arenas in the list
	while(a != NULL)
	{
		arena *tempArena = a;
		a = a->nextArena;
		// Free the memory in the arena
		free(tempArena->top);
		// Free the arena
		free(tempArena);
	}
	// free the list
	free(al);
}

// Should hold all memory arenas, and necessary synchronization primitives
typedef struct allocator {
	arenaList* al;
	liveList* ll;
	size_t numArenas;
} allocator;

// Global state variable holding the allocator that all the code operates on
static allocator alloc;

// Initalize the allocator and arena list
void my_malloc_init(size_t default_size, size_t num_arenas) 
{ 
	// Create a new arenaList and pass in the default arena size
	alloc.al = new_arenaList(default_size);
	// Create a new liveList
	alloc.ll = new_liveList();
	// Set numArenas
	alloc.numArenas = num_arenas;
	// Add in num_arenas
	for(int i = 0; i < num_arenas; i++){ addArena(alloc.al); }

	// Initialize the mutex locks
	arena *aren = alloc.al->head;
	while(aren != NULL)
	{
		pthread_mutex_init(&aren->waitlock, NULL);
		aren = aren->nextArena;
	}
}

// Free all of the arenas
void my_malloc_destroy(void)
{ 
	// Free all the arenas
	free_arena_list(alloc.al);
	// Free all the live memory
	free_liveList(alloc.ll);
}

// Allocate size memory from an arena. If the arena is full, add another arena
// Locks around arena modification (top, bump, remaining)
void* my_malloc(size_t size)
{
	// Get the current ID
	pthread_t currId = pthread_self();

	// Find the arena to go to
	int arenaIndex = currId % alloc.numArenas;

	arena *a = alloc.al->head;

	// Get the arena
	for(int i = 0; i < arenaIndex; i++)
	{
		a = a->nextArena;
	}
	
	// We lock INDIVIDUAL ARENA WAITLOCKS around the resizing and allocation 
	pthread_mutex_lock(&a->waitlock);
	// Resize if there is not enough space
	if(size > a->remaining)
	{
		// Save the live memory in the arena
		addLiveMemory(alloc.ll,a->top);
		// Create a new memory chunk twice the size of the request
		a->top = malloc(size * 2);
		// Reset bump ptr
		a->bumpPtr = a->top;
		// Reset remaining
		a->remaining = size * 2;
	}

	// Allocate
	// Get the memory chunk
	void *returnMemChunk = a->bumpPtr;
	// Update bumpPtr and remaining
	a->bumpPtr += size;
	a->remaining -= size;

	// Unlock INDIVIDUAL ARENA
	pthread_mutex_unlock(&a->waitlock);

	// Return the memory chunk
	return returnMemChunk;
}