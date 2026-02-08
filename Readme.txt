================================================================================
CSN6214 Operating Systems Assignment - Ono Card Game
================================================================================

1. COMPILATION
--------------------------------------------------------------------------------
This project can be compiled using the provided Makefile or manually via GCC.
The system requires the pthread library for multithreading support.

Option A: Using Make (Recommended)
   Run the following command in the project root directory:
   $ make

Option B: Manual Compilation
   You could compile the server and client separately:
   
   $ gcc -pthread -o server server.c
   $ gcc -o client client.c

   Note: The -pthread flag is mandatory for the server to support the logger 
   and scheduler threads.

2. HOW TO RUN & EXAMPLE COMMANDS
--------------------------------------------------------------------------------
This system requires multiple terminal windows to simulate different players
connecting to the server.

Step 1: Start the Server
   Open a terminal and run the server. It will wait 60 seconds for players.
   $ ./server

Step 2: Start Clients (Players)
   Open separate terminals for each player (minimum 2, maximum 5).
   $ ./client

   Follow the on-screen prompts to enter your player name.
   Example interaction:
   > Enter your name: Alice
   > Joined the game as Alice

Step 3: Play the Game:
   Once 2 to 5 players have joined and the countdown finishes, the game begins.

   The move pile will be displayed, as well as their own player's hand on their own specific terminal.
   Use the following commands in the client terminal.

   For Basic Moves,
   - Play card:        move <card_index>
   Example: move 1 (Plays the first card on their hand)

   - Draw a card:      draw
   (Use this if you have no playable cards)

   - Quit game:        quit

   For Advanced Moves:
   - Play Wild Card (red/blue/green/yellow):   move <card_index> <colour>
   Example: move 3 blue (Plays third card on their deck and sets the colour blue)
   Example: move 6 green (Plays sixth card on their deck and sets the colour green)

   - Declare uno:      move <card_index> uno
   Example: move 2 uno (Plays second card on their deck and declares uno)
   Note: You need to declare uno on your second last move or else it will draw 2 cards

--------------------------------------------------------------------------------
3. MODE SUPPORTED
--------------------------------------------------------------------------------
Deployment Mode: Single-machine Mode

Description:
This project is implemented for a single host. It uses Inter-Process 
Communication (IPC) via Named Pipes (FIFOs) located in /tmp/ to facilitate 
communication between the server process and client processes.

- Server uses fork() to handle incoming client connections.
- Server uses pthreads for the concurrent Logger and Round Robin Scheduler.
- Shared Memory (mmap) is used to store the Game State accessible by all processes.
- Signal Handling (SIGPIPE) is used to prevent server crashes when the client disconnects.
- Signal handling (SIGINT) is used to shut down server.

--------------------------------------------------------------------------------
4. GAME RULES SUMMARY
--------------------------------------------------------------------------------
Title: Ono (Text-Based Card Game)

Objective:
Be the first player to get rid of all your cards.

Setup:
- The game supports 2 to 5 players.
- Each player starts with 7 cards.

Gameplay Rules:
1. Matching: On your turn, you must match the top card of the discard pile 
   by either number, color, or symbol.
2. Drawing: If you cannot play a card, you must type 'draw' to pick a card 
   from the deck.
3. Power Cards:
   - Skip: The next player loses their turn.
   - Reverse: Reverses the order of play.
   - Draw Two: The next player must draw 2 cards and loses their turn.
   - Wild: Change the color of play (Red, Blue, Green, Yellow).
   - Wild Draw Four: Change the color, next player draws 4 cards continues their turn.
4. Uno Rule: A player must declare "uno" when they are down to one card. 
   Failure to do so results in a penalty of drawing 2 cards.
5. Scoring:
   - When a player wins, the game ends.
   - Scores are calculated based on cards remaining in opponents' hands.
   - Results are saved to 'scores.txt'.
   - Detailed game events are logged to 'game.log'.

Note: Once the gameplay ends, and the winner is decided, 
the user must shut down the terminal instance by using Ctrl+c.
--------------------------------------------------------------------------------
5. CLEANUP
--------------------------------------------------------------------------------
The game creates temporary pipe files in the /tmp/ directory. 
If the game crashes or is interrupted, you can manually clean up these files 
using the following command:
   $ rm /tmp/join_fifo /tmp/client_*

================================================================================
Group Members
================================================================================
1. Muhammad Yusuf bin Riduan - 251UC240TK
2. Elsa Zara binti Fakhurrazi - 251UC2502P
3. Syed Zaki Husain Wafa - 251UC2515C
4. Wan Wei Siang - 251UC2508W
