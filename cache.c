#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

static int find_cache_entry(int disk_num, int block_num){
	for(int i = 0; i < cache_size; i++){ //Iterates through all cache entries
		if(cache[i].valid && cache[i].disk_num ==disk_num && cache[i].block_num == block_num){ //Checks if the current cache entry is valid and matches the requested disk and block numbers
			return i; //Returns the index of cache entry that matches 
		}
	}
	return -1; //Returns -1 if not cache entry is found to be matching
}

static int find_least_recently_used(){
	int least_recently_used_index = -1; //Initalizes a variable and is used to store the index of least recently used cache entry  
	int min_access_time = clock;//Initializes the minimum access time to current clock value
	
	for(int i = 0; i < cache_size; i++){ //Iterates through all cache entries
		if(!cache[i].valid || cache[i].access_time < min_access_time){ // Checks if the current cache entry is invalid or has the access time less than the current minimum 
			least_recently_used_index = i;//Upadtes the least_recently_used_index to current cache index
			min_access_time = cache[i].access_time; //Updates the minimum access time to the current entry's access time
		}
	}
	return least_recently_used_index;//Returns the value of lru_index
}

int cache_create(int num_entries) {
	if (num_entries < 2 || num_entries > 4096 || cache != NULL){ //Checks if the current cache entry is out of range
		return -1; //Returns -1 indicating faliure
	}
	
	cache = (cache_entry_t *)malloc(num_entries * sizeof(cache_entry_t)); //Allocates memory for the cache based on the number of entries requested
	if(cache == NULL){ 
		return -1; //Returns -1 indicating faliure
	}
	
	cache_size = num_entries; //Sets the size of cache
	memset(cache, 0, num_entries * sizeof(cache_entry_t)); //Initializes the cache memory to zeroes
	
	return 1; //Returns 1 indicating success
}

int cache_destroy(void) {
	if(cache == NULL){ //Checks if cache does not exits
		return -1; //Returns -1 indicating faliure
	}
	
	free(cache); //Free the allocated memory for cache
	cache = NULL; //Sets the cache pointer to NULL
	cache_size = 0; //Resets the cache size to 0
	return 1; //Returns 1 indicating success
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
	int index = find_cache_entry(disk_num, block_num); //Finds the cache entry for the given disk and block number
	
	if(index == -1){ //Increments the number of queries
		num_queries++; //
		return -1; //Returns -1 indicating entry is not found
	}
	if(index != -1 && buf != NULL){ 
		memcpy(buf, cache[index].block, JBOD_BLOCK_SIZE); //Copies the data from cache to the provided buffer
		cache[index].access_time = clock++; //Updates the access time to the current clock value
		num_queries++; //Increments the number of queries
		num_hits++; //Increments the number of hits
		return 1; //Returns 1 indicating success
	}
	return -1; //Returns -1 indicating faliure
}


void cache_update(int disk_num, int block_num, const uint8_t *buf) {
	int index = find_cache_entry(disk_num, block_num); //Finds the cache entry for the given disk and block number
	if(index != -1 && buf != NULL){
		memcpy(cache[index].block, buf, JBOD_BLOCK_SIZE); //Updates the cache entry with the new data
		cache[index].access_time = clock++; //Updates the access time to the current clock value
	}
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
	if(disk_num < 0 || disk_num > 16 || block_num < 0 || block_num > 256){ //Validates the disk and block number
		return -1; //Returns -1 is out of range
	}

	int index = find_cache_entry(disk_num, block_num); //Finds the cache entry for the given disk and block number
	if(index != -1){
		return -1; //Returns -1 if the entry exits to avoid a duplicate
	}
	
	index = find_least_recently_used(); //Finds the least recently used entry
	if(index != -1 && buf != NULL){ 
		cache[index].valid = true; //Marks the cache entry as valid
		cache[index].disk_num = disk_num; //Sets the disk number
		cache[index].block_num = block_num; //Sets the block number
		memcpy(cache[index].block, buf, JBOD_BLOCK_SIZE); //Copies the data into the cache
		cache[index].access_time = clock++; //Updates the access time
		num_queries++; //Increments the number of queries
		return 1; //Returns 1 indicating success
	}
	
	return -1; //Return -1 indicating failure
}

bool cache_enabled(void) {
  	return cache != NULL && cache_size >= 0; //Checks if the cache exists and had a non-negative size
}

void cache_print_hit_rate(void) {
		fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries); //Calculates and prints the cache hit rate as a percentage
}
