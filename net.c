#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/ Define packet size for communication, sum of header length and JBOD block size.
#define PACKET_SIZE (HEADER_LEN + JBOD_BLOCK_SIZE)

// Global variable storing the client socket descriptor for server connection.
int cli_sd = -1;

// Read specified number of bytes (len) from the file descriptor (fd) into the buffer (buf).
static bool nread(int fd, int len, uint8_t *buf) {
    int bytes_read = 0;
    while (bytes_read < len) {
        int bytes = read(fd, buf + bytes_read, len - bytes_read);
        if (bytes < 0) {
            return false;  // Read error, return false
        } else if (bytes == 0) {
            return false;  // No bytes read (EOF), return false
        }
        bytes_read += bytes;  // Increment total bytes read
    }
    return true;  // Successful read of len bytes
}

// Write specified number of bytes (len) from the buffer (buf) to the file descriptor (fd).
static bool nwrite(int fd, int len, uint8_t *buf) {
    int bytes_written = 0;
    while (bytes_written < len) {
        int bytes = write(fd, buf + bytes_written, len - bytes_written);
        if (bytes < 0) {
            return false;  // Write error, return false
        }
        bytes_written += bytes;  // Increment total bytes written
    }
    return true;  // Successful write of len bytes
}

// Receive a packet from the server and extract operation code (op), return value (ret), and optional block data.
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
    uint8_t header[HEADER_LEN];
    if (!nread(sd, HEADER_LEN, header)) {
        return false;  // Failed to read header, return false
    }
    uint16_t len = ntohs(*(uint16_t *)header);  // Network to host short conversion for length
    *op = ntohl(*(uint32_t *)(header + 2));  // Network to host long conversion for operation code
    *ret = ntohs(*(uint16_t *)(header + 6));  // Network to host short conversion for return value
    if (len == HEADER_LEN + JBOD_BLOCK_SIZE && block != NULL) {
        if (!nread(sd, JBOD_BLOCK_SIZE, block)) {
            return false;  // Failed to read block, return false
        }
    }
    return true;  // Successful packet reception
}


/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
    uint8_t packet[PACKET_SIZE];
    memset(packet, 0, PACKET_SIZE);  // Clear packet buffer
    uint32_t cmd = op >> 26;  // Extract command bits by shifting
    uint16_t packet_len = (cmd == JBOD_WRITE_BLOCK && block != NULL) ? PACKET_SIZE : HEADER_LEN;  // Determine packet length
    *(uint16_t *)packet = htons(packet_len);  // Host to network short conversion for packet length
    *(uint32_t *)(packet + 2) = htonl(op);  // Host to network long conversion for operation code
    if (cmd == JBOD_WRITE_BLOCK && block != NULL) {
        memcpy(packet + HEADER_LEN, block, JBOD_BLOCK_SIZE);  // Copy block data into packet
    }
    return nwrite(sd, packet_len, packet);  // Write packet to socket descriptor
}

// Attempt to establish a connection to the server at the specified IP and port.
bool jbod_connect(const char *ip, uint16_t port) {
    struct sockaddr_in serv_addr;  // Server address structure
    memset(&serv_addr, 0, sizeof(serv_addr));  // Clear structure
    serv_addr.sin_family = AF_INET;  // Set address family to Internet
    serv_addr.sin_port = htons(port);  // Convert port number from host to network short
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        return false;  // IP address conversion failed
    }
    cli_sd = socket(AF_INET, SOCK_STREAM, 0);  // Create new socket
    if (cli_sd == -1) {
        return false;  // Socket creation failed
    }
    if (connect(cli_sd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        close(cli_sd);  // Close socket on failure
        cli_sd = -1;
        return false;  // Connection failed
    }
    return true;  // Connection established successfully
}

// Close the connection and reset the client socket descriptor.
void jbod_disconnect(void) {
    if (cli_sd != -1) {
        close(cli_sd);  // Close the socket
        cli_sd = -1;  // Reset client socket descriptor
    }
}

// Perform a JBOD operation using the provided operation code and optional block data.
int jbod_client_operation(uint32_t op, uint8_t *block) {
    if (cli_sd == -1) {
        return -1;  // No connection, return failure
    }
    if (!send_packet(cli_sd, op, block)) {
        return -1;  // Sending packet failed, return failure
    }
    uint32_t recv_op;
    uint16_t recv_ret;
    if (!recv_packet(cli_sd, &recv_op, &recv_ret, block)) {
        return -1;  // Receiving packet failed, return failure
    }
    if (recv_op != op) {
        return -1;  // Operation code mismatch, return failure
    }
    return (int)recv_ret;  // Return the server response
}
