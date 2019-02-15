#include "my_malloc.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define ONE_KB  1024
#define HALF_KB 512

#define SINGLE_ARENA  1
#define MULTI_ARENA   4

#define test(cond) \
	do { \
		if (!cond) { \
			printf("Test %s failed\n", #cond); \
			exit(1); \
		} \
	} while (0) 

#define testm(cond,message,...) \
	do { \
		if (!cond) { \
			printf(message, __VA_ARGS__); \
			exit(1); \
		} \
	} while (0)

char *test_strings[] = {
	"first test string"
	};

size_t test_string_size = sizeof(test_strings) / sizeof(test_strings[0]);

void load_the_allocator() {
  for (int i = 0; i < 1000; i++) {
		if (i % 2 == 0) {
			// Even requests create integer arrays, assign them, and check for assignment
			for (int j = 1; j < 100; j++) {
				int *arr = my_malloc(sizeof(int) * j);
				for (int k = 0; k < j; k++) {
					arr[k] = k;
				}
				for (int k = 0; k < j; k++) {
					testm((arr[k] == k), "Test 4: array allocation size iteration %d of size %d at index %d failed\n", i, j, k);
				}
			}
		} else {
			// Odd requests create strings, assign them, and check for assignment
			for (int j = 1; j < 100; j++) {
				char *ts = test_strings[j % test_string_size];
				char *s = my_malloc(strlen(ts) + 1);
				strcpy(s,ts);
				testm((strcmp(ts,s) == 0), "Test 4: string allocation iteration %d failed on string string \"%s\"\n", j, ts);
			}
		}
	} 
}

void test_serial(void) {
	my_malloc_init(ONE_KB, SINGLE_ARENA);

	// Test 1: Does my_malloc return valid memory
	test((my_malloc(HALF_KB) != NULL));

	// Test 2: Does malloc handle multiple requests
	for (int i = 0; i < 10; i++) {
		testm((my_malloc(HALF_KB) != NULL), "Test 2: my_malloc failed at index %d\n", i);
	}

	// Test 3: Does malloc handle larger requests
	for (int j = 0; j < 10; j++) {
		int *arr = my_malloc(sizeof(int) * 1000);
		for (int k = 0; k < 1000; k++) {
			arr[k] = k;
		}
		for (int k = 0; k < 1000; k++) {
			testm((arr[k] == k), "Test 3: array allocation size %d at index %d failed\n", j, k);
		}
	}

	// Test 4: Issue several requests of various sizes. Attempt an access on all memory.
	// It *may* expose an error is an access segfaults.
  load_the_allocator();
	printf("All serial logic tests passed!\n");

	// Proper cleanup of your entire allocator is a test on its own. For your sanity, the "tests" are meant to be
	// self contained. Getting the cleanup working after every single test would be very challenging. Better to
	// isolate the cases a bit.
	//
	// This means that you should properly clean everything up in my_malloc_destroy, because the
	// next test will call its own pair of my_malloc_init / my_malloc_destroy.
	my_malloc_destroy();

	printf("All serial tests passed!\n");
}

void* do_simple(void *p) {
  // Concurrent Test 1: Does malloc handled lots of smaller requests (will cause concurrent access)
	for (int j = 0; j < 10000; j++) {
		int *arr = my_malloc(sizeof(int) * 4);
		for (int k = 0; k < 4; k++) {
			arr[k] = k;
		}
		for (int k = 0; k < 4; k++) {
			testm((arr[k] == k), "Concurrent Test 1: array allocation iteration %d at failed at index %d\n", j, k);
		}
	}

  // Concurrent Test 2:  Does malloc handle the serial heavy load
  load_the_allocator();	

  return NULL;
}

void* do_heavy(void *p) {
  load_the_allocator();	
  return NULL;
}

#define N_SIMPLE_THREADS 2

void test_concurrent(void) {
	my_malloc_init(ONE_KB, MULTI_ARENA);

  pthread_t threads[N_SIMPLE_THREADS];
  for (int i = 0; i < N_SIMPLE_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, do_simple, NULL) != 0) {
      printf("Fatal Error: pthread_create failed, exiting!\n");
      exit(1);
    }
  }

  for (int i = 0; i < N_SIMPLE_THREADS; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      printf("Fatal Error: pthread_join failed, exiting!\n");
      exit(1);
    }
  }

	printf("All concurrent tests passed!\n");

	my_malloc_destroy();
}


# define N_HEAVY_THREADS 32

void test_concurrent_contention(void) {
	my_malloc_init(ONE_KB, MULTI_ARENA);

  pthread_t threads[N_HEAVY_THREADS];
  for (int i = 0; i < N_HEAVY_THREADS; i++) {
    if (pthread_create(&threads[i], NULL, do_heavy, NULL) != 0) {
      printf("Fatal Error: pthread_create failed, exiting!\n");
      exit(1);
    }
  }

  for (int i = 0; i < N_HEAVY_THREADS; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      printf("Fatal Error: pthread_join failed, exiting!\n");
      exit(1);
    }
  }

	printf("All concurrent contention tests passed!\n");

	my_malloc_destroy();
}


int main(int argc, char **argv) { 
	// The first few tests will check if you handle the very general case of 
	// single threaded allocation correctly, i.e., no threads will be created
	// other than the main process
	test_serial();

	test_concurrent();

	test_concurrent_contention();

	printf("All tests passed!\n");

	return 0;
}
