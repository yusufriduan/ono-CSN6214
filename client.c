#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define NAME_SIZE 50
#define JOIN_FIFO "/tmp/join_fifo"
#define MAX_BUFFER 1024

int main() {
    char player_name[NAME_SIZE];
    char client_fifo[64];
    char buffer[MAX_BUFFER];
     
    printf("Enter your name: ");
    fgets(player_name, NAME_SIZE, stdin);
    player_name[strcspn(player_name, "\n")] = 0;

    pid_t pid = getpid();
    snprintf(client_fifo, sizeof(client_fifo), "/tmp/client_%d", pid);
    
    unlink(client_fifo);
    if (mkfifo(client_fifo,0666) == -1) {
        perror("Failed to create client pipe.");
        return 1;
    }

    int fd = -1;
    printf("Looking for server...\n");

    while (1) {
        fd = open(JOIN_FIFO, O_WRONLY);

        if (fd != -1) {
            printf("\nConnected to the server...\n");
            break;
        }
        else {
            if (errno != ENOENT) {
                perror("Unexpected error when connecting to server.");
                unlink(client_fifo);
                return 1;
            }

            printf("Server not ready yet. Waiting...\n");
            fflush(stdout);
            sleep(1);
        }
    }

    snprintf(buffer, sizeof(buffer), "%d %s", pid, player_name);
    write(fd, buffer, strlen(buffer));
    close(fd);

    printf("Request sent! Waiting for game to start...\n");

    int my_fd = open(client_fifo, O_RDONLY); // This BLOCKS until Server connects
    if (my_fd == -1) {
        perror("Unable to open client FIFO");
        close(fd);
        unlink(client_fifo);
        return 1;
    }
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(my_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            // Received data from server!
            printf("[Server]: %s\n", buffer);
            
            // Check for game over
            if (strstr(buffer, "GAME_OVER")) break;
            
            // TODO: Add logic here to let user play a card if it's their turn
        } else if (bytes_read == 0) {
            printf("Server disconnected.\n");
            break;
        }
    }

    close(my_fd);
    unlink(client_fifo); // Clean up the FIFO
    return 0;
}
