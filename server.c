#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>

#define LOG_QUEUE_SIZE 50
#define LOG_MSG_LEN 100
#define NAME_SIZE 50
#define START_CARD_DECK 7
#define JOIN_FIFO "/tmp/join_fifo"
#define MAX_HAND_SIZE 64
#define DECK_SIZE 220
#define MAX_PLAYERS 6

typedef enum cardColor {
    CARD_COLOUR_RED = 0,
    CARD_COLOUR_BLUE = 1,
    CARD_COLOUR_GREEN = 2,
    CARD_COLOUR_YELLOW = 3,
    CARD_COLOUR_BLACK = 4 
} cardColour;

typedef enum cardType {
    CARD_NUMBER_TYPE = 0,
    CARD_SKIP_TYPE = 1,
    CARD_REVERSE_TYPE = 2,
    CARD_DRAW_TWO_TYPE = 3,
    CARD_WILD_TYPE = 4,
    CARD_WILD_DRAW_FOUR_TYPE = 5
} cardType;

typedef enum cardValue {
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

typedef struct {
  cardColour colour;
  cardType type;
  cardValue value;
} Card;

// 4 cards of each colour, of each type (+4 is 8 copies)
typedef struct deck
{
    Card deckCards[DECK_SIZE];
    uint8_t top_index;
} Deck;
int w;

typedef struct {
    char player_name[NAME_SIZE];
    pid_t pid;
    int is_active;
    Card hand_cards[MAX_HAND_SIZE];
    uint8_t hand_size;
} Player;

typedef enum GameDirection
{
    GAME_DIRECTION_NONE = 0,
    GAME_DIRECTION_LEFT = -1,
    GAME_DIRECTION_RIGHT = 1
} GameDirection;

typedef struct {
    char queue[LOG_QUEUE_SIZE][LOG_MSG_LEN];
    int head; 
    int tail;
    // semaphore function (mostly for logging)
    sem_t count;
    sem_t space_left;
    pthread_mutex_t lock;
} LogQueue;


typedef struct {
  Deck deck;
  Player players[MAX_PLAYERS];
  Card played_cards[DECK_SIZE];
  uint8_t current_card_idx;

  LogQueue logger;
  int num_players;
  int current_player;
  int next_player;
  int winner_pid; // 0 = No winner determined
  int direction; // 1 = Clockwise | 1 == Anti-clockwise
  int game_over;

  // Sync prmitives for the Game State
  pthread_mutex_t game_lock;
  pthread_cond_t turn_cond;

  // store player moves (card being played)
  char stored_move[64];      // input from player is stored
  int move_ready;          // 0 = not ready, 1 = waiting
  int player_move_index;  //index of player send

} GameState;
GameState *shm;

void enqueue_log(char *msg);
void *logger_thread_func(void *arg);
void player_add_card(Player *player, Card new_card);
void check_for_uno(Player *player, GameState *game);
void decide_next_player(GameState *game);
void deckInit(Deck *onoDeck);
void deckShuffle(Deck *onoDeck);
void execute_card_effect(Card *c, GameState *game, int wild_colour);
void execute_wild_card(GameState *game, int wild_colour);
bool check_for_winner(Player *player);
bool player_turn(int player_index, GameState *game);
bool player_play_card(Player *player, uint8_t card_played, GameState *game, int wild_colour);
//void gameplay(GameState *game);
//void game_init(GameState *game);

Card deckDraw(Deck *onoDeck)
{
    if (onoDeck->top_index >= DECK_SIZE)
    {
        onoDeck->top_index = 0;
        deckShuffle(onoDeck);
    }

    return onoDeck->deckCards[onoDeck->top_index++];
}

#ifndef CARD
#define CARD

// cards
// For displaying cards
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
    else if (card->colour == top_card->colour || top_card->colour == CARD_COLOUR_BLACK)
    {
        return true;
    }

    // Check for same card type
    else if ((card->type != CARD_NUMBER_TYPE && top_card->type != CARD_NUMBER_TYPE) && card->type == top_card->type)
    {
        return true;
    }

    // Check for wild cards
    else if (card->type == CARD_WILD_TYPE || card->type == CARD_WILD_DRAW_FOUR_TYPE)
    {
        return true;
    }

    else
    {
        return false;
    }
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

void display_card(Card *card)
{
    printf("[%d (%s)]", card->value, get_colour_name(card->colour));
}

void execute_reverse_card(GameState *game)
{
    game->direction *= -1;

    int n = game->num_players;
    game->next_player = (game->current_player + game->direction + n) % n;
}

void execute_skip_card(GameState *game)
{
    int n = game->num_players;
    game->next_player = (game->next_player + game->direction + n) % n;
}

void execute_draw_two_card(GameState *game)
{
    game->players[game->next_player].hand_cards[game->players[game->next_player].hand_size++] = deckDraw(&game->deck);
    game->players[game->next_player].hand_cards[game->players[game->next_player].hand_size++] = deckDraw(&game->deck);

    int n = game->num_players;
    game->next_player = (game->current_player + game->direction + n) % n;
}

void execute_wild_card(GameState *game, int wild_colour)
{
    if (wild_colour >= 1 && wild_colour <= 4) {
        // Map 1-4 to enum 0-3 (Red=0, Blue=1, Green=2, Yellow=3)
        game->played_cards[game->current_card_idx].colour = (cardColour)(wild_colour - 1);
    } else {
        // Default to Red if user didn't pick
        game->played_cards[game->current_card_idx].colour = CARD_COLOUR_RED;
    }
}

// For when a player plays a power card/wild card
void execute_card_effect(Card *c, GameState *game, int wild_colour)
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
        execute_wild_card(game, wild_colour);
        break;
    case CARD_VALUE_WILD_DRAW_FOUR:
        execute_wild_card(game, wild_colour);
        for(int i = 0; i < 4; i++) {
            int victim = (game->current_player + game->direction + game->num_players) % game->num_players;
            player_add_card(&game->players[victim], deckDraw(&game->deck));
        }
        break;
    default:
        break;
    }
}

#endif // CARD


// Jason
#ifndef PLAYER
#define PLAYER
#define MAX_HAND_SIZE 64

void player_init(Player *player, const char *name)
{
    strncpy(player->player_name, name, NAME_SIZE - 1); // copy the name given into the player itself
    player->hand_size = 0;                             // Cards given during start of round
}

bool player_play_card(Player *player, uint8_t card_played, GameState *game, int wild_colour)
{
    Card *chosen_card = &player->hand_cards[card_played];
    Card *top_card = &game->played_cards[game->current_card_idx];

    if (!playable_card(chosen_card, top_card))
    {
        printf("> Invalid card played! Card not playable on top of pile.\n");
        return false;
    }
    game->played_cards[++game->current_card_idx] = *chosen_card;

    player->hand_cards[card_played] = player->hand_cards[player->hand_size - 1]; // Replace played card with last card
    player->hand_size--;

    execute_card_effect(&game->played_cards[game->current_card_idx], game, wild_colour);
    return true;
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


bool player_turn(int player_index, GameState *game) {
  Player *P = &game->players[player_index];
  char *cmd = game->stored_move;
  char msg[100];

  if (strncmp(cmd, "DRAW", 4) == 0) {
    printf("> You draw a card...");
    Card card_drawn = deckDraw(&game->deck);
    player_add_card(P, card_drawn);

    //Sending message to game_log
    snprintf(msg, sizeof(msg), "Player %d drew a card", player_index);
    enqueue_log(msg);
    return true;
  } else if (strncmp(cmd, "MOVE", 4) == 0) {
    int card_index;
    int colour_code = 0; // Default 0

    // Get the Index and colour after cmd "MOVE"
    sscanf(cmd + 5, "%d %d", &card_index, &colour_code);

    // Since client sends 1-based index, Server need to convert to 0-based
    int actual_index = card_index - 1;

    if (actual_index >= 0 && actual_index < P->hand_size) {

        bool move_successful = player_play_card(P, actual_index, game, colour_code);

        if (move_successful)
        {
            // Send the card Details for logging
            Card *c = &P->hand_cards[actual_index];
            snprintf(msg, sizeof(msg), "Player %d played %d (%s)", player_index, c->value, get_colour_name(c->colour));
            enqueue_log(msg);

            check_for_uno(P, game);
            return true;
        } else {
            return false;
        }
        
    
    } else {
    printf("Error: Player %d tried invalid index %d\n", player_index, card_index);
    snprintf(msg, sizeof(msg), "Player %d tried invalid play index %d", player_index, card_index);
    enqueue_log(msg);

    //PENALTY: DRAW A CARD
    player_add_card(P, deckDraw(&game->deck));

    return false;
    }
  }
  return false;
}

#endif

void format_card_to_string(Card *c, char *buffer) {
    const char *colour = get_colour_name(c->colour);

    if (c->type == CARD_NUMBER_TYPE) {
        sprintf(buffer, "%d (%s)", c->value, colour);
    } else {
        const char *type_str;
        switch (c->type) {
            case CARD_SKIP_TYPE: type_str = "SKIP"; break;
            case CARD_REVERSE_TYPE: type_str = "REVERSE"; break;
            case CARD_DRAW_TWO_TYPE: type_str = "DRAW TWO"; break;
            case CARD_WILD_TYPE: type_str = "WILD"; break;
            case CARD_WILD_DRAW_FOUR_TYPE: type_str = "WILD DRAW FOUR"; break;
            default: type_str = "UNKNOWN"; break;
        }
        sprintf(buffer, "%s %s", type_str, colour);
    }
}

void check_for_uno(Player *player, GameState *game)
{   
    char declared_uno[3];
    if(player->hand_size == 1){
        printf("> You are about to win! Type 'Uno' or draw two cards: ");
        scanf("%s", declared_uno);

        if(strcmp(declared_uno, "uno") != 0){
            printf("> Uh oh! You didn't say Uno! You'll now draw two cards!");
            player->hand_cards[player->hand_size++] = deckDraw(&game->deck);
            player->hand_cards[player->hand_size++] = deckDraw(&game->deck);
        }
        else{
            printf("> Player %d has declared uno!", game->current_player);
        }
    }
}

// void gameplay(GameState *game)
// {
//     while (!game->winner_pid)
//     {
//         player_turn(&game->players[game->current_player], game);
//         if (check_for_winner(&game->players[game->current_player]))
//             game->game_over = 1;
//         decide_next_player(game);
//     }
//     printf("{ %s WINS! }\n\n", game->players[game->winner_pid].player_name);
// }

// void game_init(GameState *game)
// {
//     deckInit(&game->deck);
//     game->direction = 1;
//     game->current_player = 0;
//     game->current_card_idx = 0;
//     game->next_player = game->current_player + game->direction;

//     // Give out 7 cards to each player
//     for (int i = 0; i < MAX_PLAYERS; i++)
//     {
//         for (int j = 0; j < 7; j++)
//         {
//             player_add_card(&game->players[i], deckDraw(&game->deck));
//         }
//     }

//     // Play the starting card
//     game->played_cards[0] = deckDraw(&game->deck);
//     while ((game->played_cards[0].type != CARD_NUMBER_TYPE))
//     {
//         deckShuffle(&game->deck);
//         game->played_cards[0] = deckDraw(&game->deck);
//     }
//     game->current_card_idx = 1;

//     //gameplay(game);
// }



void deckInit(Deck *onoDeck)
{
    uint8_t top_index = 0;
    int w = 0;
    // Adding coloured cards into the deck
    for (int i = 0; i < 4; i++)
    {
        // Number cards (0-9)
        for (int j = 0; j < 10; j++)
        {
          // Creating 4 copies of the number card
          for (int k = 0; k < 4; k++) {
            onoDeck->deckCards[top_index++] = (Card){
              .colour = (cardColour)i,
              .type = CARD_NUMBER_TYPE,
              .value = (uint8_t)j};
            }
        }

        // Power cards (Skip [10], Reverse[11], Draw Two[12])
        for (int l = 10; l < 13; l++)
        {
            // Ensuring the card is given its correct cardType
            switch (l)
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
            for (int m= 1; m < 4; m++)
            {
                onoDeck->deckCards[top_index++] = (Card){
                    .colour = (cardColour)i,
                    .type = (cardType)w,
                    .value = (uint8_t)l};
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
    for (int i = DECK_SIZE - 1; i > 0; i--)
    {
        int j = rand() % (i + 1); // Generate Random Number between 0 to DECK_SIZE (220)
        Card temp = onoDeck->deckCards[i];
        onoDeck->deckCards[i] = onoDeck->deckCards[j];
        onoDeck->deckCards[j] = temp;
    }
}

void player_add_card(Player *player, Card new_card)
{
    if (player->hand_size < MAX_HAND_SIZE)
    {
        player->hand_cards[player->hand_size] = new_card;
        player->hand_size++;
    }
}

bool check_for_winner(Player *player)
{
    if(player->hand_size == 0){
        shm->winner_pid = player->pid;
        shm->game_over = 1;
        return true;
    }
  return false;
}


void decide_next_player(GameState *game)
{
    int n = game->num_players;

    // if (game->next_player >= n)
    // {
    //     game->current_player = 0; // Wrap back to the first player.
    // }
    // else if (game->next_player < 0)
    // {
    //     game->current_player = n - 1; // Wrap back to the last player.
    // }
    // else
    // {
    //     game->current_player = game->next_player;
    // }
    game->current_player = (game->next_player + n) % n;
    game->next_player = (game->current_player + game->direction) % n;
}

// Pass logging mechanism
void enqueue_log(char *msg) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timed_msg[LOG_MSG_LEN];

    int time_len = strftime(timed_msg, LOG_MSG_LEN, "[%H:%M:%S] ", t);
    snprintf(timed_msg + time_len, LOG_MSG_LEN - time_len, "%s", msg);

    sem_wait(&shm->logger.space_left);
    pthread_mutex_lock(&shm->logger.lock);    
    
    strncpy(shm->logger.queue[shm->logger.tail], timed_msg, LOG_MSG_LEN - 1);
    shm->logger.queue[shm->logger.tail][LOG_MSG_LEN - 1] = '\0';
    
    shm->logger.tail = (shm->logger.tail + 1) % LOG_QUEUE_SIZE;
    
    pthread_mutex_unlock(&shm->logger.lock);
    sem_post(&shm->logger.count);
}

// THE ACTUAL THREAD LOGGING
void *logger_thread_func(void *arg) {
    LogQueue *lq = (LogQueue *)arg;

    FILE *fp = fopen("game_log", "a");
    if (!fp) {
        perror("Logger failed to open file");
        pthread_exit(NULL);
    }

    while (1) {
        sem_wait(&lq->count);

        pthread_mutex_lock(&lq->lock);

        char buffer[LOG_MSG_LEN];
        strcpy(buffer, lq->queue[lq->head]);

        lq->head = (lq->head + 1) % LOG_QUEUE_SIZE;

        pthread_mutex_unlock(&lq->lock);
        sem_post(&lq->space_left);

        fprintf(fp, "%s\n", buffer);
        fflush(fp); // Ensure it saves immediately

        if (strcmp(buffer, "SERVER_SHUTDOWN") == 0) break;
    }

    fclose(fp);
    return NULL;
}

// If player disconnect
void handle_disconnect(int player_index, char *player_name, int fd) {
    char log_msg[LOG_MSG_LEN];

    snprintf(log_msg, LOG_MSG_LEN, "DISCONNECT: Player %s (Index %d) left.", player_name, player_index);
    enqueue_log(log_msg);

    if (fd != -1) {
        close(fd);
    }

    printf("Player %s disconnected. Cleaning up child process...\n", player_name);
    shm->players[player_index].is_active = 0;
    exit(0);
}

void update_player_client(int player_index, int pipe_fd) {
    char msg[1024] = {0};
    char card_str[50];

    // Send top card on pile
    Card *top_card = &shm->played_cards[shm->current_card_idx];
    format_card_to_string(top_card, card_str);

    strcat(msg, "PILE:");
    strcat(msg, card_str);
    strcat(msg, "\n");

    // Send player's hand
    strcat(msg, "HAND:");
    Player *P = &shm->players[player_index];

    for (int i = 0; i < P->hand_size; i++) {
        format_card_to_string(&P->hand_cards[i], card_str);
        strcat(msg, card_str);

        // Add comma if not the last card
        if (i < P->hand_size - 1) {
            strcat(msg, ",");
        }
    }
    strcat(msg, "\n");

    // Send the message to the player
    write(pipe_fd, msg, strlen(msg));
}

// Game starts
int main() {
    int num_players = 0;
    int countdown = 60;
    char player_names[5][NAME_SIZE];
    int client_pid_list[5];
    char raw_buffer[128];

    int player_pipes[5];
    for (int i=0; i<5; i++)
        player_pipes[i] = -1;

    shm = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if(shm == MAP_FAILED) {
      perror("mmap failed");
      return 1;
    }

    // Initialize Sync Premitives in Shared Memory
    sem_init(&shm->logger.count, 1, 0); 
    sem_init(&shm->logger.space_left, 1, LOG_QUEUE_SIZE);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->logger.lock, &attr);

    pthread_mutex_init(&shm->game_lock, &attr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shm->turn_cond, &cattr);

    pthread_t log_tid;
    pthread_create(&log_tid, NULL, logger_thread_func, (void *)&shm->logger);

    unlink(JOIN_FIFO); // Remove any existing FIFO
    if (mkfifo(JOIN_FIFO, 0666) == -1) {
        perror("Failed to create join FIFO");
        enqueue_log("Failed to create join FIFO");
        return 1;
    }

    int join_fd = open(JOIN_FIFO, O_RDONLY | O_NONBLOCK);
    if (join_fd == -1) {
        perror("Failed to open join FIFO");
        enqueue_log("Failed to open join FIFO");
        unlink(JOIN_FIFO);
        return 1;
    }

    enqueue_log("Server started, waiting for players to join.");

    for (int i = countdown; i > 0; i--) { 
        int n = read(join_fd, raw_buffer, sizeof(raw_buffer)-1);
        if (n > 0) {
            raw_buffer[n] = '\0';
            char *line = strtok(raw_buffer, "\n");

            while (line != NULL) {
                int client_pid;
                char temp_name[NAME_SIZE];
                if (sscanf(line, "%d %49[^\n]", &client_pid, temp_name) == 2) {
                    if (num_players < 5) {
                        strncpy(player_names[num_players], temp_name, NAME_SIZE);
                        client_pid_list[num_players] = client_pid;

                        strncpy(shm->players[num_players].player_name, temp_name, NAME_SIZE);
                        shm->players[num_players].pid = client_pid;
                        shm->players[num_players].is_active = 1;
                        shm->players[num_players].hand_size = 0;

                        char log_msg[LOG_MSG_LEN];
                        snprintf(log_msg, LOG_MSG_LEN, "Player joined: %s (PID: %d)", temp_name, client_pid);
                        enqueue_log(log_msg);
                        
                        char client_fifo[64];
                        snprintf(client_fifo, 64, "/tmp/client_%d", client_pid);
                        int c_fd = open(client_fifo, O_WRONLY);
                        if (c_fd != -1) {
                            write(c_fd, "Welcome to the game!\n", 21);
                            player_pipes[num_players] = c_fd;
                        }

                        num_players++;
                        shm->num_players = num_players;
                    }
                }
                line = strtok(NULL, "\n");
            }
        }

        printf("\033[2J\033[H"); // Clear Screen and move cursor to top
        printf("Initiated Server Client of Ono Card Ono Game\n");
        printf("Waiting for players to join...\n");
        printf("Current players: %d\n", num_players);
        printf("Time left to join: %d seconds\n", i - 1);
        printf("Players:\n");
        for(int p=0; p<num_players; p++)
            printf(" - %s\n", player_names[p]);
        fflush(stdout);

        if (i == 0 && (num_players > 1 || num_players < 6))
            break;

        if (num_players == 5)
            break;
            
        sleep(1);
    }
    close(join_fd);
    unlink(JOIN_FIFO);

    printf("\033[2J\033[H");
    fflush(stdout);

    // initialize game state
    shm->direction = 1;
    shm->current_player = 0;
    shm->current_card_idx = 0;
    shm->next_player = shm->current_player + shm->direction;

    if (num_players > 1 && num_players < 6) {
        printf("Game starting with %d players!\n", num_players);
        for (int i = 0; i < num_players; i++) {
            printf("Player %s\n", player_names[i]); // and then followed with line 213 for loop
        }

        deckInit(&shm->deck);
        deckShuffle(&shm->deck);
        
        for (int i=0; i<num_players; i++){
            for (int c=0; c<START_CARD_DECK; c++)
                player_add_card(&shm->players[i], deckDraw(&shm->deck));
        }

        for (int i=0; i < num_players; i++) {
            pid_t pid = fork();

            if (pid == 0) {
                char client_in_fifo[64];
                snprintf(client_in_fifo, 64, "/tmp/client_%d_in", client_pid_list[i]);
                int player_fd = open(client_in_fifo, O_RDONLY);
                if(player_fd == -1){
                    perror("Child failed to open player input pipe");
                    exit(1);
                }

                while (1) {
                    char buffer[1024];
                    int n = read(player_fd, buffer, sizeof(buffer));

                    if (n > 0) {
                        // Process Game Move [ELSA PART]

                        buffer[n] = '\0'; //convert from bytes to C string 
                        pthread_mutex_lock(&shm->game_lock); // freeze game state, prevent others from altering

                        // copy move into gamestate (store the move)
                        strncpy(shm->stored_move, buffer, (sizeof(shm->stored_move)-1)); //minus 1 to exlude '\0'

                        shm->move_ready = 1; // ready for next player's move 
                        shm->player_move_index = i; // updates the player who sent the moves

                        pthread_cond_signal(&shm->turn_cond); // wake up parent process
                        pthread_mutex_unlock(&shm->game_lock); // unfreeze gamestate, allow others to alter

                    } else if (n == 0) {
                        handle_disconnect(i, player_names[i], player_fd);
                    }
                }
                exit(0);
            }
        }
        // Round Robin Scheduler, Parent Process [ELSA PART] 
        // Check winner

        while(!shm->game_over){

            pthread_mutex_lock(&shm->game_lock);// locks game
            uint8_t player = shm->current_player;     
            pthread_mutex_unlock(&shm->game_lock);// unlocks game

            update_player_client(player, player_pipes[player]);

            // inform next player of their move
            write(player_pipes[player], "TURN\n", 5); 

            pthread_mutex_lock(&shm->game_lock);
            // wait until player finished move + make sure its the same player signaling
            while(!shm->move_ready || shm->player_move_index != player){

                pthread_cond_wait(&shm->turn_cond, &shm->game_lock);
            }
            //apply move changes 
            bool success = player_turn(player, shm);
            shm->move_ready = 0;

            if (success) {
                if (check_for_winner(&shm->players[player])) {
                    shm->game_over = true;

                    for (int i = 0; i < shm->num_players; i++)
                    {
                        write(player_pipes[i], "GAME_OVER\n", 10);
                    }
                    
                } else {
                    decide_next_player(shm);

                    for(int p=0; p< shm->num_players ; p++) {
                        if (shm->players[p].is_active) {
                            update_player_client(p, player_pipes[p]);
                        }
                    }
                }
            } else {
                // Invalid move, player draws a card as penalty
                write(player_pipes[player], "INVALID_MOVE\n", 13);
                player_add_card(&shm->players[player], deckDraw(&shm->deck));
                update_player_client(player, player_pipes[player]);
            }
            pthread_mutex_unlock(&shm->game_lock);
        }
    } else {    
        fprintf(stderr, "Number of players must be between 2 and 5.\n");
        return 1;
    }

    printf("Game over! Winner PID: %d\n", shm->winner_pid);
    enqueue_log("SERVER_SHUTDOWN");
    pthread_join(log_tid, NULL);
    return 0;
}

