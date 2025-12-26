#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
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
    int num_players = 0;
    int countdown = 60;
    char player_names[5][NAME_SIZE];

    FILE *log_gl = fopen("game_log.txt", "a");
    if (!log_gl)
    {
        perror("Failed to open log file");
        return 1;
    }
    

    mkfifo(JOIN_FIFO, 0666);
    int join_fd = open(JOIN_FIFO, O_RDONLY | O_NONBLOCK);
    char player_name[NAME_SIZE];

    
    for (int i = countdown; i > 0; i--) {
        int n = read(join_fd, player_name, sizeof(player_name));
        if (n > 0 && num_players < 5) {
            player_name[n] = '\0';
            strncpy(player_names[num_players], player_name, NAME_SIZE);
            num_players++;
            fprintf(log_gl, "Player joined: %s\n", player_name);
            fflush(log_gl);
        }

        printf("\033[2J\033[H"); // Clear Screen and move cursor to top
        printf("Initiated Server Client\n");
        printf("Waiting for players to join...\n");
        printf("Current players: %d\n", num_players);
        printf("Time left to join: %d seconds\n", i - 1);
        fflush(stdout);

        if (num_players == 5)
            break;        

        sleep(1);

    }
    close(join_fd);
    unlink(JOIN_FIFO);

    printf("\033[2J\033[H");

    if (num_players > 2 || num_players < 6) {
        printf("Game starting with %d players!\n", num_players);
        for (int i = 0; i < num_players; i++) {
            printf("Player %s\n", player_names[i]);
        }
    } else {    
        fprintf(stderr, "Number of players must be between 3 and 5.\n");
        return 1;
    }
    fclose(log_gl);
    return 0;
}