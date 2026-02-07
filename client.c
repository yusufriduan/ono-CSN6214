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
#include <ctype.h>


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
    p += 5; // skip "PILE:"
    while (*p == ' ') p++;

    printf("\n=== Pile Card ===\n");
    // Create a temporary buffer to print ONLY the current line
    char card_text[64];
    size_t len = strcspn(p, "\n"); // Calculate length until the next newline
    if (len >= sizeof(card_text)) len = sizeof(card_text) - 1;
    
    strncpy(card_text, p, len);
    card_text[len] = '\0'; // Ensure null-termination

    printf("%s\n", card_text);
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
    size_t len = strcspn(p, "\n");
    if (len >= sizeof(temp))
    {
        len = sizeof(temp) - 1;
    }
    
    strncpy(temp, p, len);
    temp[len] = '\0';

    int idx = 1;
    int hand_size_checker = 0; //just for uno checking
    char *token = strtok(temp, ",");
    while (token != NULL) {
        while (*token == ' ') token++;   // skip leading spaces
        if (strlen(token) > 0) {
            printf("%d: %s\n", idx++, token);
            hand_size_checker++;
        }
        token = strtok(NULL, ",");
    }
    if(hand_size_checker == 2){
        printf("\n> You have 2 cards remaining! (move <something> uno) to declare uno!\n");
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
            // printf("[Server]: %s\n", buffer);
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

                    if (strncmp(move, "move", 4) == 0)
                    {
                        int card_index;
                        char colour_str[20];
                        int colour_code = 0; // Default 0
                        int uno_declaration = 0; //To detect if UNO is declared

                        int args = sscanf(move + 5, "%d %s", &card_index, colour_str);

                        if (args == 2) {
                            // User declares uno
                            if (strcasecmp(tolower(colour_str), "uno") == 0) {
                                uno_declaration = 1;
                                snprintf(out, sizeof(out), "MOVE %d %d\n", card_index, uno_declaration);
                                break;
                            }
                            // User provided a colour
                            else if (strcasecmp(tolower(colour_str), "red") == 0) {
                                colour_code = 1;
                                snprintf(out, sizeof(out), "MOVE %d %d\n", card_index, colour_code);
                            } else if (strcasecmp(tolower(colour_str), "blue") == 0) {
                                colour_code = 2;
                                snprintf(out, sizeof(out), "MOVE %d %d\n", card_index, colour_code);
                            } else if (strcasecmp(tolower(colour_str), "green") == 0) {
                                colour_code = 3;
                                snprintf(out, sizeof(out), "MOVE %d %d\n", card_index, colour_code);
                            } else if (strcasecmp(tolower(colour_str), "yellow") == 0) {
                                colour_code = 4;
                                snprintf(out, sizeof(out), "MOVE %d %d\n", card_index, colour_code);
                            }
                        } else {
                            // No colour provided
                            snprintf(out, sizeof(out), "MOVE %d 0\n", card_index);
                        }
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