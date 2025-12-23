# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <time.h>
# include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NAME_SIZE 50
#define START_CARD_DECK 8
#define JOIN_FIFO "/tmp/join_fifo"

typedef struct {
    char cards;
    char names[5][NAME_SIZE];
    int current_player;
    int game_over;
} Player;

void initialize_game(Player *player) {
    player->cards = START_CARD_DECK;
    for (int i = 0; i < 5; i++) {
        snprintf(player->names[i], NAME_SIZE, "Player%d", i + 1);
    }
    player->current_player = 0;
    player->game_over = 0;
}

int main() {
    printf("Initiated Server Client\n");
    int num_players = 0;
    int countdown = 15;

    mkfifo(JOIN_FIFO, 0666);
    int join_fd = open(JOIN_FIFO, O_RDONLY | O_NONBLOCK);
    char player_name[NAME_SIZE];

    printf("Waiting for players to join...\n");
    for (int i = countdown; i > 0; i--) {
        int n = read(join_fd, player_name, sizeof(player_name));
        if (n > 0 && num_players < 5) {
            player_name[n] = '\0';
            printf("Player joined: %s\n", player_name);
            num_players++;
        }
        sleep(1);
        printf("Time left to join: %d seconds\r", i - 1);
        fflush(stdout);
    }
    close(join_fd);
    unlink(JOIN_FIFO);

    if (num_players < 3 || num_players > 5) {
        fprintf(stderr, "Number of players must be between 3 and 5.\n");
        return 1;
    }


    return 0;
}