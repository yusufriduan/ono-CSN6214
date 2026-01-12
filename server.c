#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

#define LOG_QUEUE_SIZE 50
#define LOG_MSG_LEN 100
#define NAME_SIZE 50
#define START_CARD_DECK 8
#define JOIN_FIFO "/tmp/join_fifo"

typedef struct {
    char queue[LOG_QUEUE_SIZE][LOG_MSG_LEN];
    int head; // Where the Logger Thread reads from
    int tail; // Where Child Processes write to
    
    // Synchronization primitives
    sem_t count;         // Counts how many messages are waiting (Start at 0)
    sem_t space_left;    // Counts how much space is left (Start at LOG_QUEUE_SIZE)
    pthread_mutex_t lock; // Protects the indices (head/tail)
} LogQueue;

typedef struct {
    char cards;
    char names[5][NAME_SIZE];
    int current_player;
    int game_over;
} Player;

LogQueue logger;

void enqueue_log(char *msg) {
    sem_wait(&logger.space_left);          // Wait for space
    pthread_mutex_lock(&logger.lock);      // Lock
    
    // Copy message
    strncpy(logger.queue[logger.tail], msg, LOG_MSG_LEN - 1);
    logger.queue[logger.tail][LOG_MSG_LEN - 1] = '\0';
    
    logger.tail = (logger.tail + 1) % LOG_QUEUE_SIZE; // Advance tail
    
    pthread_mutex_unlock(&logger.lock);    // Unlock
    sem_post(&logger.count);               // Wake up logger thread
}

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
    char player_name[NAME_SIZE];
    char raw_buffer[128];

    sem_init(&logger.count, 0, 0); 
    sem_init(&logger.space_left, 0, LOG_QUEUE_SIZE);
    pthread_mutex_init(&logger.lock, NULL);

    pthread_t log_tid;
    pthread_create(&log_tid, NULL, logger_thread_func, (void *)&logger);

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

            int client_pid;
            if (sscanf(raw_buffer, "%d %49[^\n]", &client_pid, player_name) == 2) {
                if (num_players < 5) {
                    strncpy(player_names[num_players], player_name, NAME_SIZE);
                    num_players++;
                    
                    char log_msg[LOG_MSG_LEN];
                    snprintf(log_msg, LOG_MSG_LEN, "Player joined: %s (PID: %d)", player_name, client_pid);
                    enqueue_log(log_msg);
                    
                    char client_fifo[64];
                    snprintf(client_fifo, 64, "/tmp/client_%d", client_pid);
                    int c_fd = open(client_fifo, O_WRONLY);
                    if (c_fd != -1) {
                        write(c_fd, "Welcome to the game!\n", 8);
                        close(c_fd);
                    }
                }
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

        if (i == 0 && (num_players > 2 || num_players < 6))
            break;

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

    enqueue_log("Server shutting down.");
    pthread_join(log_tid, NULL);
    return 0;
}
