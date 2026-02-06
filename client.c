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
#include "server.c"


#define NAME_SIZE 50
#define JOIN_FIFO "/tmp/join_fifo"
#define MAX_BUFFER 1024
#ifndef CARD
#define CARD

// Cards
typedef enum cardColour
{
    CARD_COLOUR_RED = 0,
    CARD_COLOUR_BLUE = 1,
    CARD_COLOUR_GREEN = 2,
    CARD_COLOUR_YELLOW = 3,
    CARD_COLOUR_BLACK = 4 // Wild Cards
} cardColour;

typedef enum cardValue
{
    CARD_VALUE_NONE = -1,
    CARD_VALUE_0 = 0,
    CARD_VALUE_1 = 1,
    CARD_VALUE_2 = 2,
    CARD_VALUE_3 = 3,
    CARD_VALUE_4 = 4,
    CARD_VALUE_5 = 5,
    CARD_VALUE_6 = 6,
    CARD_VALUE_7 = 7,
    CARD_VALUE_8 = 8,
    CARD_VALUE_9 = 9,
    CARD_VALUE_SKIP = 10,
    CARD_VALUE_REVERSE = 11,
    CARD_VALUE_DRAW_TWO = 12,
    CARD_VALUE_WILD = 13,
    CARD_VALUE_WILD_DRAW_FOUR = 14
} cardValue;

typedef enum cardType
{
    CARD_NUMBER_TYPE = 0,
    CARD_SKIP_TYPE = 1,
    CARD_REVERSE_TYPE = 2,
    CARD_DRAW_TWO_TYPE = 3,
    CARD_WILD_TYPE = 4,
    CARD_WILD_DRAW_FOUR_TYPE = 5
} cardType;

typedef struct Card
{
    cardColour colour;
    cardType type;
    cardValue value;
} Card;

// For displaying cards

bool playable_card(Card *card, Game* game)
{

    // Card is playable when
    // 1. Same value as top card
    // 2. Same colour as top card
    // 3. The card is a wild card5
    // 4. Same card type as top card

    // Check if same value
    if ((card->value != CARD_VALUE_NONE && game->played_cards[game->current_card].value != CARD_VALUE_NONE) && card->value == game->played_cards[game->current_card]->value)
    {
        return true;
    }

    // Check for same colour
    else if (card->colour == game->played_cards[game->current_card]->colour || game->played_cards[game->current_card]->colour == CARD_COLOUR_BLACK)
    {
        return true;
    }

    // Check for same card type
    else if ((card->type != CARD_NUMBER_TYPE && game->played_cards[game->current_card].type != CARD_NUMBER_TYPE) && card->type == game->played_cards[game->current_card]->type)
    {
        return true;
    }

    // Check for wild cards
    else if (card->type == CARD_WILD_TYPE)
    {
        return true;
    }

    else
    {
        return false;
    }
}

void display_card(Card *card)
{
    printf("[%s (%s)]", card->value, get_colour_name(card->colour));
}

#endif // CARD

// Jason
#ifndef PLAYER
#define PLAYER
#define MAX_HAND_SIZE 64

typedef struct player
{
    char player_name[NAME_SIZE];
    Card hand_cards[MAX_HAND_SIZE];
    uint8_t hand_size;
} Player;

void player_init(Player *player, const char *name)
{
    strncpy(player->player_name, name, NAME_SIZE - 1); // copy the name given into the player itself
    player->hand_size = 0;                             // Cards given during start of round
}

void player_add_card(Player *player, Card new_card)
{
    if (player->hand_size < MAX_HAND_SIZE)
    {
        player->hand_cards[player->hand_size] = new_card;
        player->hand_size++;
    }
}

void player_play_card(Player *player, uint8_t card_played, Game *game)
{
    while (!playable_card(&player->hand_cards[card_played], game->played_cards[game->current_card]))
    {
        printf("> Card %d is not a playable card, please pick another card", card_played);
        int temp_input;
        scanf("%d", &temp_input);
        card_played = (uint8_t)temp_input;
        player_play_card(player, card_played, game);
        return;
    }
    game->played_cards[++game->current_card] = player->hand_cards[player->hand_size - 1];
    player->hand_size--;
    printf("> A %d(%s) has been played!", game->played_cards[game->current_card].value, get_colour_name(&game->played_cards[game->current_card]));
    execute_card_effect(&game->played_cards[game->current_card], game);
}

// For displaying the Player's hand
int player_check_hand(Player *player)
{
    for (int i = 0; i < player->hand_size - 1; i++)
    {
        Card hand_card = player->hand_cards[i];
        display_card(&hand_card);
    }
    return 0;
}

void player_turn(Player *player, Game *game)
{
    int selected_card = 0;

    player_check_hand(player);
    printf("> Your turn! Select a card (1-%d) or enter 0 to draw: ", player->hand_size);
    scanf("%d", &selected_card);

    while (selected_card > player->hand_size)
    {
        printf("> Invalid move, Try again.\n Select a card (1-%d) or enter 0 to draw: ", player->hand_size);
        scanf("%d", &selected_card);
    }

    if (selected_card == 0)
    {
        printf("> You draw a card...");
        Card card_drawn = deckDraw(&game->deck);
        player_add_card(player, card_drawn);
    }
    else
    {
        printf("> You play a card...");
        player_play_card(player, selected_card, game);
    }
    check_for_uno(player, game);
}

#endif


int is_turn_msg(const char *msg)
{
    return (strstr(msg, "TURN") != NULL) || (strstr(msg, "Your turn") != NULL);
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

    int write_fd = open(server_fifo, O_WRONLY | O_NONBLOCK);
    if(write_fd == -1){
        perror("Failed to connect to server input");
        return 1;
    }

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(my_fd, buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        
        if (bytes_read > 0) {
            // Received data from server!
            printf("[Server]: %s\n", buffer);

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
