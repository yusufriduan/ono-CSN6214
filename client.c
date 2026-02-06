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
void display_card(Card *card)
{
}

bool playable_card(Card *card, Card *top_card)
{

    // Card is playable when
    // 1. Same value as top card
    // 2. Same colour as top card
    // 3. The card is a wild card5
    // 4. Same card type as top card

    // Check if same value
    if ((card->value != CARD_VALUE_NONE && top_card->value != CARD_VALUE_NONE) && card->value == top_card->value)
    {
        return true;
    }

    // Check for same colour
    else if (card->colour == top_card->colour || card->colour == CARD_COLOUR_BLACK)
    {
        return true;
    }

    // Check for same card type
    else if ((card->type != CARD_NUMBER_TYPE && top_card->type != CARD_NUMBER_TYPE) && card->type == top_card->type)
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
#endif // CARD

#ifndef DECK
#define DECK

#define DECK_SIZE 220
// 4 cards of each colour, of each type (+4 is 8 copies)
typedef struct deck
{
    Card deckCards[DECK_SIZE];
    uint8_t top_index;
} Deck;
int w;

void deckInit(Deck *onoDeck)
{
    uint8_t top_index = 0;

    // Adding coloured cards into the deck
    for (int i = 0; i < 4; i++)
    {

        // Number cards (0-9)
        for (int j = 0; i < 10; i++)
        {

            // Creating 4 copies of the number card
            for (int k = 0; k < 4; k++)
            {
                onoDeck->deckCards[top_index++] = (Card){
                    .colour = (cardColour)i,
                    .type = CARD_NUMBER_TYPE,
                    .value = (uint8_t)j};
            }
        }

        // Power cards (Skip [10], Reverse[11], Draw Two[12])
        for (int j = 10; i < 13; i++)
        {
            // Ensuring the card is given its correct cardType
            switch (j)
            {
            case 10:
                w = 1;
                break;
            case 11:
                w = 2;
                break;
            case 12:
                w = 3;
                break;
            }
            // Creating 4 copies of the power cards
            for (int k = 1; k < 4; k++)
            {
                onoDeck->deckCards[top_index++] = (Card){
                    .colour = (cardColour)i,
                    .type = (cardType)w,
                    .value = (uint8_t)j};
            }
        }
    }

    // 4 Wild Cards
    for (int l = 0; l < 4; l++)
    {
        onoDeck->deckCards[top_index++] = (Card){
            .colour = CARD_COLOUR_BLACK,
            .type = CARD_WILD_TYPE,
            .value = CARD_VALUE_WILD};
    }

    // 8 Wild Card Draw Fours
    for (int m = 0; m < 8; m++)
    {
        onoDeck->deckCards[top_index++] = (Card){
            .colour = CARD_COLOUR_BLACK,
            .type = CARD_WILD_DRAW_FOUR_TYPE,
            .value = CARD_VALUE_WILD_DRAW_FOUR};
    }

    // Ensure pointer is now at top card
    onoDeck->top_index = 0;
}

void deckShuffle(Deck *onoDeck)
{
    // Random Number Generator Seed
    unsigned int seed = (unsigned int)time(NULL);

    for (int i = DECK_SIZE - 1; i > 0; i--)
    {

        int j = (int)((double)rand_r(&seed) / (i + 1) * (i + 1)); // Generate Random Number between 0 to DECK_SIZE (220)

        Card temp = onoDeck->deckCards[i];
        onoDeck->deckCards[i] = onoDeck->deckCards[j];
        onoDeck->deckCards[j] = temp;
    }

    uint8_t top_index = 0;
}

const char *get_colour_name(cardColour c)
{
    switch (c)
    {
    case CARD_COLOUR_RED:
        return "Red";
    case CARD_COLOUR_BLUE:
        return "Blue";
    case CARD_COLOUR_GREEN:
        return "Green";
    case CARD_COLOUR_YELLOW:
        return "Yellow";
    case CARD_COLOUR_BLACK:
        return "Wild";
    default:
        return "Unknown";
    }
}

// To-do: Add Reshuffling deck
Card deckDraw(Deck *onoDeck, Game *game)
{

    if (onoDeck->top_index >= DECK_SIZE)
    {
        onoDeck->top_index = 0;
        deckShuffle(onoDeck);
    }

    return (onoDeck->deckCards[onoDeck->top_index++]);
}

#endif // DECK

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
    while (!playable_card(&player->hand_cards[card_played], &game->played_cards[game->current_card]))
    {
        printf("> Card %d is not a playable card, please pick another card", card_played);
        int temp_input;
        scanf("%d", &temp_input);
        card_played = (uint8_t)temp_input;
        player_play_card(player, card_played, &game->played_cards[game->current_card]);
        return;
    }
    game->played_cards[++game->current_card] = player->hand_cards[player->hand_size - 1];
    player->hand_size--;
    printf("> A %d(%s) has been played!", game->played_cards[game->current_card].value, get_colour_name(&game->played_cards[game->current_card]));
    execute_card_effect(&game->played_cards[game->current_card], &game);
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
        Card card_drawn = deckDraw(&game->deck, game);
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

// Syed Zaki
#ifndef GAME
#define GAME
#define MAX_PLAYERS 6

typedef enum GameDirection
{
    GAME_DIRECTION_NONE = 0,
    GAME_DIRECTION_LEFT = -1,
    GAME_DIRECTION_RIGHT = 1
} GameDirection;

typedef struct game
{
    Deck deck;
    Player players[MAX_PLAYERS];
    Card played_cards[DECK_SIZE];
    GameDirection gameFlow;
    uint8_t current_card, current_player, next_player, winner;
    bool decided_winner;
} Game;

void game_init(Game *game)
{
    deckInit(&game->deck);
    game->gameFlow = 1;
    game->current_player = 0;
    game->current_card = 0;
    game->next_player = game->current_player + game->gameFlow;

    // Give out 7 cards to each player
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            player_add_card(&game->players[i], deckDraw(&game->deck, game));
        }
    }

    // Play the starting card
    game->played_cards[0] = deckDraw(&game->deck, game);
    while ((game->played_cards[0].type != CARD_NUMBER_TYPE))
    {
        deckShuffle(&game->deck);
        game->played_cards[0] = deckDraw(&game->deck, game);
    }
    game->current_card = 1;

    gameplay(game);
}

void gameplay(Game *game)
{
    while (!game->decided_winner)
    {
        player_turn(&game->players[game->current_player], game);
        game->decided_winner = (&game->players[game->current_player], game);
        decide_next_player(game);
    }
    printf("{ %s WINS! }\n\n", game->players[game->winner].player_name);
}

bool check_for_winner(Player *player, Game *game)
{
    if(player->hand_size == 0){
        game->winner = game->current_player;
        return 1;
    }
}

void check_for_uno(Player *player, Game *game)
{   
    char declared_uno[3];
    if(player->hand_size == 1){
        printf("> You are about to win! Type 'Uno' or draw two cards: ");
        scanf("%s", declared_uno);

        if(declared_uno != "uno"){
            printf("> Uh oh! You didn't say Uno! You'll now draw two cards!");
            player->hand_cards[player->hand_size++] = deckDraw(&game->deck, game);
            player->hand_cards[player->hand_size++] = deckDraw(&game->deck, game);
        }
        else{
            printf("> Player %d has declared uno!", game->current_player);
        }
    }
}

void decide_next_player(Game *game)
{
    if (game->next_player >= MAX_PLAYERS)
    {
        game->current_player = 0; // Wrap back to the first player.
    }
    else if (game->next_player < 0)
    {
        game->current_player = MAX_PLAYERS - 1; // Wrap back to the last player.
    }
    else
    {
        game->current_player = game->next_player;
        game->next_player += game->gameFlow;
    }
}

// For when a player plays a power card/wild card
void execute_card_effect(Card *c, Game *game)
{
    switch (c->value)
    {
    case CARD_VALUE_SKIP:
        execute_skip_card(game);
        break;
    case CARD_VALUE_REVERSE:
        execute_reverse_card(game);
        break;
    case CARD_VALUE_DRAW_TWO:
        execute_draw_two_card(game);
        break;
    case CARD_VALUE_WILD:
        execute_wild_card(game);
        break;
    case CARD_VALUE_WILD_DRAW_FOUR:
        execute_wild_card_draw_four(game);
        break;
    default:
        break;
    }
}

static void execute_reverse_card(Game *game)
{
    switch (game->gameFlow)
    {
    case 1:
        game->gameFlow = -1;
        break;
    case -1:
        game->gameFlow = 1;
        break;
    default:
        game->gameFlow = 1;
    }
}

static void execute_skip_card(Game *game)
{
    game->current_player+= game->gameFlow;
    game->next_player+= game->gameFlow;
}

static void execute_draw_two_card(Game *game)
{
    game->players[game->next_player].hand_cards[game->players[game->next_player].hand_size++] = deckDraw(&game->deck, game);
    game->players[game->next_player].hand_cards[game->players[game->next_player].hand_size++] = deckDraw(&game->deck, game);
}

static void execute_wild_card(Game *game)
{
    uint8_t chosen_colour;
    fprint("> Choose a colour, 1: Red, 2: Blue, 3: Green, 4: Yellow: ");
    scanf("%d", chosen_colour);
    switch(chosen_colour){
        case 1: game->played_cards[game->current_card].colour = CARD_COLOUR_RED; break;
        case 2: game->played_cards[game->current_card].colour = CARD_COLOUR_BLUE; break;
        case 3: game->played_cards[game->current_card].colour = CARD_COLOUR_GREEN; break;
        case 4: game->played_cards[game->current_card].colour = CARD_COLOUR_YELLOW; break;
        default: fprint("Unknown colour picked");
    }
}

static void execute_wild_card_draw_four(Game *game)
{
    execute_wild_card(game);
    for(int i = 0; i < 4; i++)
        game->players[game->next_player].hand_cards[game->players[game->next_player].hand_size++] = deckDraw(&game->deck, game);
}

#endif // GAME

int is_turn_msg(const char *msg)
{
    return (strstr(msg, "TURN") != NULL) || (strstr(msg, "Your turn") != NULL);
}

int main()
{
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
    if (write_fd == -1)
    {
        perror("Failed to connect to server input");
        return 1;
    }

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(my_fd, buffer, sizeof(buffer));

        if (bytes_read > 0)
        {
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

            // TODO: Add logic here to let user play a card if it's their turn
        }
        else if (bytes_read == 0)
        {
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
