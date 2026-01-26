#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#define NAME_SIZE 50
#define JOIN_FIFO "/tmp/join_fifo"
#define MAX_BUFFER 1024
#ifndef CARD
#define CARD

//Cards
typedef enum cardColor {
    CARD_COLOUR_RED = 0,
    CARD_COLOUR_BLUE = 1,
    CARD_COLOUR_GREEN = 2,
    CARD_COLOUR_YELLOW = 3,
    CARD_COLOUR_BLACK = 4 // Wild Cards
} cardColour;

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

typedef enum cardType {
    CARD_NUMBER_TYPE = 0,
    CARD_SKIP_TYPE = 1,
    CARD_REVERSE_TYPE = 2,
    CARD_DRAW_TWO_TYPE = 3,
    CARD_WILD_TYPE = 4,
    CARD_WILD_DRAW_FOUR_TYPE = 5
} cardType;

typedef struct Card {
    cardColour colour;
    cardType type;
    cardValue value;
} Card;

bool card_IsPlayable(const Card* card, const Card* top_card){

    //Card is playable when
    //1. Same value as top card
    //2. Same colour as top card
    //3. The card is a wild card5
    //4. Same card type as top card

    //Check if same value
    if((card->value != CARD_VALUE_NONE && top_card->value != CARD_VALUE_NONE) && card->value == top_card->value){
        return true;
    }

    //Check for same colour
    else if(card->colour == top_card->colour || card->colour == CARD_COLOUR_BLACK){
        return true;
    }

    //Check for same card type
    else if((card->type != CARD_NUMBER_TYPE && top_card->type != CARD_NUMBER_TYPE) && card->type == top_card->type){
        return true;
    }

    //Check for wild cards
    else if(card->colour == CARD_COLOUR_BLACK || card->type == CARD_WILD_TYPE){
        return true;
    }

    else{
        return false;
    }

}
#endif // CARD

#ifndef DECK
#define DECK

//4 cards of each colour, of each type (+4 is 8 copies)
Card onoDeck[220] = {};

void deckShuffle(Card* onoDeck){
    onoDeck[1];
}

#endif //DECK

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
    if (mkfifo(client_fifo,0666) == -1) {
        perror("Failed to create client pipe.");
        return 1;
    }

    int fd = -1;
    printf("Looking for server...\n");

    while (1) {
        // CLient found server
        fd = open(JOIN_FIFO, O_WRONLY);

        if (fd != -1) {
            printf("\nConnected to the server...\n");
            break;
        }
        else {
            if (errno != ENOENT) {
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

    snprintf(buffer, sizeof(buffer), "%d %s", pid, player_name);
    write(fd, buffer, strlen(buffer));
    close(fd);

    printf("Request sent! Waiting for game to start...\n\n");

    int my_fd = open(client_fifo, O_RDONLY); //This BLOCKS until Server connects
    if (my_fd == -1) {
        perror("Unable to open client FIFO");
        close(fd);
        unlink(client_fifo);
        return 1;
    }

    char server_fifo[64];
    snprintf(server_fifo, sizeof(server_fifo), "/tmp/client_%d_in", pid);

    unlink(server_fifo);
    if(mkfifo(server_fifo, 0666) == -1){
        perror("Failed to create server input pipe");
        return 1;
    }

    int write_fd = open(server_fifo, O_WRONLY);
    if(write_fd == -1){
        perror("Failed to connect to server input");
        return 1;
    }
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(my_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            // Received data from server!
            printf("[Server]: %s\n", buffer);
            
            // Check for game over
            if (strstr(buffer, "GAME_OVER")) break;
            
            // TODO: Add logic here to let user play a card if it's their turn
        } else if (bytes_read == 0) {
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
