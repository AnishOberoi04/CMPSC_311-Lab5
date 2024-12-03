/* Author: Anish Oberoi    
   Date: 4/15/2024
    */
    
    
    
/***
 *      ______ .___  ___. .______     _______.  ______              ____    __   __  
 *     /      ||   \/   | |   _  \   /       | /      |            |___ \  /_ | /_ | 
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'              __) |  | |  | | 
 *    |  |     |  |\/|  | |   ___/   \   \    |  |                  |__ <   | |  | | 
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.             ___) |  | |  | | 
 *     \______||__|  |__| | _|   |_______/     \______|            |____/   |_|  |_| 
 *                                                                                   
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "cache.h"
#include "net.h"

int checker = 0; //initalizes a variable chekcer with the value 0 which later is used to check if the disk is mounted or not. 

uint32_t id_checker(int DiskID, int BlockID, int Command, int Reserved){ // function to help bit shifting. 

	if(DiskID < 0 || DiskID > 16 || BlockID < 0 || BlockID > 256 || Command < 0 || Command > 7 || Reserved != 0){
		return 0;
	}
	uint32_t id_placement; // variable that would store the final value. 
	
	int disk = DiskID << 28; //variable disk is initialized that stores the value of DiskID after bit shiting 28 places.
	int block = BlockID << 20; //variable block is initialized that stores the value of BlockID after bit shiting 20 places.
	int commands = Command << 14; //variable commands is initialized that stores the value of Command after bit shiting 14 places.
	int reserves = Reserved << 0; //variable reserves is initialized that stores the value of Reserved after bit shiting 0 places.
	
	id_placement = disk|block|commands|reserves; //stores the final value of with the proper placements of the IDs.
	
	return id_placement; //returns the variable id_placement

}


int mdadm_mount(void) {
  /* YOUR CODE */
	if (checker == 0){ // checks if the disk is unmounted
		checker = 1;// changes the value of checker to 1, thus mounting it.
		int mount = jbod_client_operation(id_checker(0, 0, JBOD_MOUNT, 0), NULL); //initalizes the variable mount as the value of the function id_checker with given parameters including JBOD_MOUNT which makes mounts the needle on the disk.
		if(mount == -1){
			return -1;
		} else {
			return 1;
		}
	}
	
	return -1;	
}

int mdadm_unmount(void) {
  /* YOUR CODE */
	if (checker == 1){ // checks if the disk is mounted
		checker = 0;// changes the value of checker to 0, thus unmounting it.
		int unmount = jbod_client_operation(id_checker(0, 0, JBOD_UNMOUNT, 0), NULL); //initalizes the variable unmount as the value of the function id_checker with given parameters including JBOD_UNMOUNT which makes unmounts the needle on the disk.
		if(unmount == -1){
			return -1;
		} else {
			return 1;
		}
	}

	return -1;
	
}

int seek_to_disk(uint32_t disk_num){ // Function to seek the specified disk
	uint32_t disk_p = id_checker(disk_num,0,JBOD_SEEK_TO_DISK,0); //Seeks the specified disk and stores it in a variable.
	if(jbod_client_operation(disk_p,NULL) == -1){
		return -1; //returns -1 if seeking the disk fails
	}
	return 0; //returns 0 if seeking the disk succeeds
}

int seek_to_block(uint32_t block_num){ // Function to seek the specified block
	uint32_t block_p = id_checker(0,block_num,JBOD_SEEK_TO_BLOCK,0); //Seeks the specified block and stores it in a variable.
	if(jbod_client_operation(block_p,NULL) == -1){
		return -1; //returns -1 if seeking the blockk fails
	}
	return 0; //returns 0 if seeking the block succeeds
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    // Check if the disk is mounted
    if (checker == 0) {
        return -1; // Disk is unmounted
    }

    // Check if the address is within the disk capacity
    if (addr + len > 1048576) {
        return -1; // Address is out of bounds
    }

    // Check if the length is larger than 1024
    if (len > 1024) {
        return -1; // Length exceeds maximum allowed
    }

    // Check if buf is NULL when length is greater than 0
    if (len > 0 && buf == NULL) {
        return -1; // Invalid buffer
    }

    int bytes_read = 0;
    while (bytes_read < len) {
        // Calculate the current disk, block, and offset within the block
        uint32_t curr_disk = addr / 65536;
        uint32_t curr_block = (addr % 65536) / 256;
        uint32_t off_block = (addr % 256);
        uint8_t temp_buf[256]; // Temporary buffer to store block data
        
        if(cache_lookup(curr_disk, curr_block, temp_buf) == 1){ //Attempts to lookup the current block in cache
        	int bytes_to_copy = (len - bytes_read) < (256 - off_block) ? (len - bytes_read) : (256 - off_block); // Calculate the number of bytes to copy
	        memcpy(buf + bytes_read, temp_buf + off_block, bytes_to_copy); // Copy data from the temporary buffer to the output buffer
	        bytes_read += bytes_to_copy; // Update the total number of bytes read
	        addr += bytes_to_copy; // Update the address pointer
	        continue;
        }

        // Seek to the current disk and block
        if (seek_to_disk(curr_disk) == -1 || seek_to_block(curr_block) == -1) {
            return -1; // Seek error
        }
	
        // Read the block data
        if (jbod_client_operation(id_checker(0, 0, JBOD_READ_BLOCK, 0), temp_buf) == -1) {
            return -1; // Read error
        }

        // Calculate the number of bytes to copy
        int bytes_to_copy = (len - bytes_read) < (256 - off_block) ? (len - bytes_read) : (256 - off_block);

        // Copy data from the temporary buffer to the output buffer
        memcpy(buf + bytes_read, temp_buf + off_block, bytes_to_copy);

        bytes_read += bytes_to_copy; // Update the total number of bytes read
        addr += bytes_to_copy; // Update the address pointer
        cache_insert(curr_disk, curr_block, temp_buf); //Inserts the read block into cache
    }

    return bytes_read; // Return the total number of bytes read
}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
    // Check if the disk is mounted
    if (checker == 0) {
        return -1; // Disk is unmounted
    }

    // Check if the address is within the disk capacity
    if (addr + len > 1048576) {
        return -1; // Address is out of bounds
    }

    // Check if the length is larger than 1024
    if (len > 1024) {
        return -1; // Length exceeds maximum allowed
    }

    // Check if buf is NULL when length is greater than 0
    if (len > 0 && buf == NULL) {
        return -1; // Invalid buffer
    }

    int bytes_written = 0;
    while (bytes_written < len) {
        // Calculate the current disk, block, and offset within the block
        uint32_t curr_disk = addr / 65536;
        uint32_t curr_block = (addr % 65536) / 256;
        uint32_t off_block = addr % 256;
        uint32_t write_len = (len - bytes_written) < (256 - off_block) ? (len - bytes_written) : (256 - off_block);

        // Seek to the current disk and block
        if (seek_to_disk(curr_disk) == -1 || seek_to_block(curr_block) == -1) {
            return -1; // Seek error
        }

        uint8_t temp_buf[256];
        
        if(cache_lookup(curr_disk, curr_block, temp_buf) == -1){ //Checks if the current block is not in cache
        	if (jbod_client_operation(id_checker(0, 0, JBOD_READ_BLOCK, 0), temp_buf) == -1) { // Read the block data
           		return -1; // Read error
        	}
        }

        // Copy data from the input buffer to the temporary buffer
        memcpy(temp_buf + off_block, buf + bytes_written, write_len);

        // Seek to the appropriate disk and block again (in case they were modified during memcpy)
        if (seek_to_disk(curr_disk) == -1 || seek_to_block(curr_block) == -1) {
            return -1; // Seek error
        }
        
        // Write the modified block back to the disk
        if (jbod_client_operation(id_checker(0, 0, JBOD_WRITE_BLOCK, 0), temp_buf) == -1) {
            return -1; // Write error
        }

	cache_update(curr_disk, curr_block, temp_buf); //Updates the cache with current blocks data from temp_buf
        bytes_written += write_len;
        addr += write_len; // Update the address pointer
    }

    return len; // Return the total length of data written
}
