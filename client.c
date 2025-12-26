#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define NAME_SIZE 50
#define JOIN_FIFO "/tmp/join_fifo"

typedef struct {
    char name[NAME_SIZE];
    // Add other player fields as needed
} Player;

int main() {
    char player_name[NAME_SIZE];
    printf("Enter your name: ");
    fgets(player_name, NAME_SIZE, stdin);
    // Remove newline character
    player_name[strcspn(player_name, "\n")] = 0;

    int fd = open(JOIN_FIFO, O_WRONLY);
    if (fd == -1) {
        perror("Unable to open join FIFO");
        return 1;
    }

    write(fd, player_name, strlen(player_name));
    close(fd);

    printf("Joined the game as %s\n", player_name);
    return 0;



    Player player;
    // initialize_game(&player); // Uncomment if you have this function defined

    // printf("Game starting with %d players!\n", num_players); // Uncomment if num_players is defined
    // Game logic would go here...

    return 0;
}