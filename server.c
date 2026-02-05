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
#include <sys/mman.h>

#define LOG_QUEUE_SIZE 50
#define LOG_MSG_LEN 100
#define NAME_SIZE 50
#define START_CARD_DECK 8
#define JOIN_FIFO "/tmp/join_fifo"

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
    char cards;
    char names[5][NAME_SIZE];
    int is_active;
    int current_player;
    int game_over;
} Player;

typedef struct {
  Player players[5];
  LogQueue logger;
  int num_players;
  int current_player;
  int winner_pid; // 0 = No winner determined
  int direction; // 1 = Clockwise | 1 == Anti-clockwise

  // Sync prmitives for the Game State
  pthread_mutex_t game_lock;
  pthread_cond_t turn_cond;
} GameState;
GameState *shm;


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
    exit(0);
}

// Game starts
int main() {
    int num_players = 0;
    int countdown = 60;
    char player_names[5][NAME_SIZE];
    int client_pid_list[5];
    char player_name[NAME_SIZE];
    char raw_buffer[128];

    shm = mmap(NULL, sizeof(GameState), PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_ANONYMOUS, -1, 0);

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

          int client_pid;
            if (sscanf(raw_buffer, "%d %49[^\n]", &client_pid, player_name) == 2) {
                if (num_players < 5) {
                    strncpy(player_names[num_players], player_name, NAME_SIZE);
                    client_pid_list[num_players] = client_pid;
                    num_players++;
                    
                    char log_msg[LOG_MSG_LEN];
                    snprintf(log_msg, LOG_MSG_LEN, "Player joined: %s (PID: %d)", player_name, client_pid);
                    enqueue_log(log_msg);
                    
                    char client_fifo[64];
                    snprintf(client_fifo, 64, "/tmp/client_%d", client_pid);
                    int c_fd = open(client_fifo, O_WRONLY);
                    if (c_fd != -1) {
                        write(c_fd, "Welcome to the game!\n", 21);
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

        for (int i=0; i < num_players; i++) {
            pid_t pid = fork();

            if (pid == 0) {
                char client_fifo[64];
                snprintf(client_fifo, 64, "/tmp/client_%d_in", client_pid_list[i]);

                int player_fd = open(client_fifo, O_RDONLY);

                if(player_fd == -1){
                    perror("Child failed to open player input pipe");
                    exit(1);
                }

                while (1) {
                    char buffer[1024];
                    int n = read(player_fd, buffer, sizeof(buffer));

                    if (n > 0) {
                        // Process Game Move, Determine winner [ELSA PART]
                    } else if (n == 0) {
                        handle_disconnect(i, player_names[i], player_fd);
                    }
                }
                exit(0);
            }
        }

        // Round Robin Scheduler, Parent Process [ELSA PART]
    } else {    
        fprintf(stderr, "Number of players must be between 2 and 5.\n");
        return 1;
    }

    enqueue_log("Server shutting down.");
    pthread_join(log_tid, NULL);
    return 0;
}
