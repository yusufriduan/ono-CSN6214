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
#define START_CARD_DECK 8
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

typedef struct game
{
    Deck deck;
    Player players[MAX_PLAYERS];
    Card played_cards[DECK_SIZE];
    GameDirection gameFlow;
    uint8_t current_card, current_player, next_player, winner;
    bool decided_winner;
} Game;

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

void deckInit(Deck *onoDeck)
{
    uint8_t top_index = 0;
    int w = 0;
    // Adding coloured cards into the deck
    for (int i = 0; i < 4; i++)
    {
        // Number cards (0-9)
        for (int j = 0; j < 10; i++)
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
        for (int l = 10; l < 13; i++)
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

bool playable_card(Card *card, Game* game)
{
    // Card is playable when
    // 1. Same value as top card
    // 2. Same colour as top card
    // 3. The card is a wild card5
    // 4. Same card type as top card


    // Check if same value
    if ((card->value != CARD_VALUE_NONE && game->played_cards[game->current_card].value != CARD_VALUE_NONE) && card->value == game->played_cards[game->current_card].value)
    {
        return true;
    }

    // Check for same colour
    else if (card->colour == &game->played_cards[game->current_card].colour || &game->played_cards[game->current_card].colour == CARD_COLOUR_BLACK)
    {
        return true;
    }

    // Check for same card type
    else if ((card->type != CARD_NUMBER_TYPE && game->played_cards[game->current_card].type != CARD_NUMBER_TYPE) && card->type == &game->played_cards[game->current_card].type)
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

void player_play_card(Player *player, uint8_t card_played, Game *game)
{
    while (!playable_card(&player->hand_cards[card_played], &game->played_cards[game->current_card]))
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

Card deckDraw(Game* game)   
{
    if (game->deck.deckCards[game->deck.top_index] >= DECK_SIZE)
    {
        game->deck.top_index = 0;
        deckShuffle(&game->deck);
    }

    //In the case the drawn card is playable, automatically play it
    if(playable_card(&game->deck.deckCards[game->deck.top_index], game)){
        player_play_card(&shm->players[game->current_player], game->deck.top_index, game);
        return (Card){.value = CARD_VALUE_NONE, .colour = CARD_COLOUR_BLACK, .type = CARD_NUMBER_TYPE};
    }
    return game->deck.deckCards[game->deck.top_index++];
}

void player_add_card(Player *player, Card new_card)
{
    if (player->hand_size < MAX_HAND_SIZE)
    {
        player->hand_cards[player->hand_size] = new_card;
        player->hand_size++;
    }
}

bool check_for_winner(Player *player, Game *game)
{
    if(player->hand_size == 0){
        shm->winner_pid = player->pid;
        shm->game_over = 1;
        return true;
    }
  return false;
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

    if (num_players > 1 && num_players < 6) {
        printf("Game starting with %d players!\n", num_players);
        for (int i = 0; i < num_players; i++) {
            printf("Player %s\n", player_names[i]); // and then followed with line 213 for loop
        }

        deckInit(&shm->deck);
        deckShuffle(&shm->deck);
        
        for (int i=0; i<num_players; i++){
            for (int c=0; c<7; c++)
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
                        pthead_mutex_lock(&shm->game_lock); // freeze game state, prevent others from altering

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
            } else {
                if (player_pipes[i] != -1)
                    close(player_pipes[i]);
            }            
        }
        // Round Robin Scheduler, Parent Process [ELSA PART] 
        // Check winner

        while(!shm->game_over){

            pthread_mutex_lock(&shm->game_lock);// locks game
            uint8_t player = shm->current_player;     
            pthread_mutex_unlock(&shm->game_lock);// unlocks game

            // inform next player of their move
            write(player_pipes[player], "TURN\n", 5); 

            pthread_mutex_lock(&shm->game_lock);
            while(!shm->move_ready){

                // wait until player finished move
                pthread_cond_wait(&shm->turn_cond);
            }
            pthread_mutex_unlock(&shm->game_lock);// unlocks game

            //apply move changes 
            apply_move(player, shm->stored_move);
            shm->move_ready = 0;

            pthread_mutex_lock(&shm->game_lock);// locks game

            //check for winner, if yes then do update 
            if(check_for_winner(player, shm)){

            shm->game_over = true;
            printf("The winner of the game is %s !", );
            break;

            }
        }
        //updates current player for game 
        decide_next_player((Game*)shm);
        pthread_mutex_unlock(&shm->game_lock);// unlocks game

){
        }
        enqueue_log("Server shutting down.");
    } else {    
        fprintf(stderr, "Number of players must be between 2 and 5.\n");
        return 1;
    }

    pthread_join(log_tid, NULL);
    return 0;
}


