#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>


#define NAME_SIZE 50
#define JOIN_FIFO "/tmp/join_fifo"
#define MAX_BUFFER 1024
#define MAX_HAND_SIZE 64
#define DECK_SIZE 220
#define MAX_PLAYERS 6

int is_turn_msg(const char *msg)
{
    return (strstr(msg, "TURN") != NULL) || (strstr(msg, "Your turn") != NULL);
}

static void show_top(const char *line) {
    // line e.g. : "PILE: RED 5"
    const char *p = strstr(line, "PILE:");
    if (!p) return;
    p += 4; // skip "PILE:"
    while (*p == ' ') p++;

    printf("\n=== Pile Card ===\n");
    printf("%s\n", p);
}

static void show_hand(const char *line) {
    // line e.g. : "HAND: RED 3, BLUE SKIP, GREEN 9"
    const char *p = strstr(line, "HAND:");
    if (!p) return;
    p += 5; // skip "HAND:"
    while (*p == ' ') p++;

    printf("\n=== Your Hand ===\n");

    // Make a copy so strtok doesn't destroy original buffer
    char temp[MAX_BUFFER];
    snprintf(temp, sizeof(temp), "%s", p);

    int idx = 1;
    char *token = strtok(temp, ",");
    while (token != NULL) {
        while (*token == ' ') token++;   // skip leading spaces
        printf("%d) %s\n", idx++, token);
        token = strtok(NULL, ",");
    }
}

static void game_display(const char *msg) {
    // Calling both. If msg doesn't contain PILE/HAND it just prints nothing extra.
    show_top(msg);
    show_hand(msg);
}

int main() {
    // Server initialization
    char player_name[NAME_SIZE];
    char client_fifo[64];
    char buffer[MAX_BUFFER];

    printf("Enter your name: ");
    fgets(player_name, NAME_SIZE, stdin);
    player_name[strcspn(player_name, "\n")] = 0;

    // Get the process ID for the piping procedure
    pid_t pid = getpid();
    snprintf(client_fifo, sizeof(client_fifo), "/tmp/client_%d", pid);

    // If the piping exist, unlink
    unlink(client_fifo);
    if (mkfifo(client_fifo, 0666) == -1)
    {
        perror("Failed to create client pipe.");
        return 1;
    }

    int fd = -1;
    printf("Looking for server...\n");

    while (1)
    {
        // CLient found server
        fd = open(JOIN_FIFO, O_WRONLY);

        if (fd != -1)
        {
            printf("\nConnected to the server...\n");
            break;
        }
        else
        {
            if (errno != ENOENT)
            {
                perror("Unexpected error when connecting to server.");
                unlink(client_fifo);
                return 1;
            }

            // Client could not find server
            printf("Server not ready yet. Waiting...\n");
            fflush(stdout);
            sleep(1);
        }
    }

    snprintf(buffer, sizeof(buffer), "%d %s\n", pid, player_name);
    write(fd, buffer, strlen(buffer));
    close(fd);

    printf("Request sent! Waiting for game to start...\n\n");

    int my_fd = open(client_fifo, O_RDONLY); // This BLOCKS until Server connects
    if (my_fd == -1)
    {
        perror("Unable to open client FIFO");
        close(fd);
        unlink(client_fifo);
        return 1;
    }

    char server_fifo[64];
    snprintf(server_fifo, sizeof(server_fifo), "/tmp/client_%d_in", pid);

    unlink(server_fifo);
    if (mkfifo(server_fifo, 0666) == -1)
    {
        perror("Failed to create server input pipe");
        return 1;
    }

    int write_fd = open(server_fifo, O_WRONLY);
    if(write_fd == -1){
        perror("Failed to connect to server input");
        return 1;
    }

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(my_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read == 0) {
            printf("Server disconnected.\n");
            break;
        }
        if (bytes_read < 0) {
            perror("read");
            break;
        }
        buffer[bytes_read] = '\0';
        
        if (bytes_read > 0) {
            // Received data from server!
            printf("[Server]: %s\n", buffer);
            game_display(buffer);

            // Check for game over
            if (strstr(buffer, "GAME_OVER"))
                break;

            if (is_turn_msg(buffer))
            {
                char move[128];

                printf("Your move (move <something> / draw / quit): ");
                fflush(stdout);

                if (fgets(move, sizeof(move), stdin) == NULL)
                {
                    break;
                }
                move[strcspn(move, "\n")] = 0;

                // quit
                if (strcmp(move, "quit") == 0 || strcmp(move, "q") == 0)
                {
                    write(write_fd, "QUIT\n", 5);
                    break;
                }

                // draw
                if (strcmp(move, "draw") == 0)
                {
                    write(write_fd, "DRAW\n", 5);
                }
                else
                {
                    // move
                    char out[180];

                    if (strncmp(move, "move", 5) == 0)
                    {
                        snprintf(out, sizeof(out), "MOVE %s\n", move + 5);
                    }
                    else
                    {
                        snprintf(out, sizeof(out), "MOVE %s\n", move);
                    }

                    write(write_fd, out, strlen(out));
                }
            }
        } 
        else if (bytes_read == 0) {
            printf("Server disconnected.\n");
            break;
        }
    }

    close(write_fd);
    unlink(server_fifo);
    close(my_fd);
    unlink(client_fifo);
    return 0;
}