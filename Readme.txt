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
   If you do not have the Makefile ready, compile the server and client separately:
   
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
   Open separate terminals for each player (minimum 3, maximum 5).
   $ ./client

   Follow the on-screen prompts to enter your player name.
   Example interaction:
   > Enter your name: Alice
   > Joined the game as Alice

Step 3: Play the Game
   Once 3 to 5 players have joined and the countdown finishes, the game begins.
   Follow the instructions in the client terminal to play your cards.

3. MODE SUPPORTED
--------------------------------------------------------------------------------
Deployment Mode: Single-machine Mode

Description:
This project is implemented for a single host. It uses Inter-Process 
Communication (IPC) via Named Pipes (FIFOs) located in /tmp/ to facilitate 
communication between the server process and client processes.

- Server uses fork() to handle incoming client connections.
- Server uses pthreads for the concurrent Logger and Round Robin Scheduler.

4. GAME RULES SUMMARY
--------------------------------------------------------------------------------
Title: Ono (Text-Based Card Game)

Objective:
Be the first player to get rid of all your cards.

Setup:
- The game supports 3 to 5 players.
- Each player starts with 8 cards.

Gameplay:
- Players take turns in a Round-Robin order managed by the server.
- On your turn, you must match the top card of the discard pile by either
  number or color.
- If you cannot play a card, you must draw from the deck.
- Special action cards (Skip, Reverse, Draw Two) may be implemented depending
  on the current game state.

Winning:
The game ends when a player successfully plays their last card. The game log will be stored on 'game_log'.

================================================================================
Group Members
================================================================================
1. Muhammad Yusuf bin Riduan - 251UC240TK
2. Elsa Zara binti Fakhurrazi - 251UC2502P
3. Syed Zaki Husain Wafa - 251UC2515C
4. Wan Wei Siang - 251UC2508W
