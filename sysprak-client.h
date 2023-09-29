#pragma once
#include "config.h"
#include <unistd.h>
#include <stdbool.h>
#define STRING_BUFFER_LENGTH 256
#define PLAYER_BUFFER_LENGTH 10
#define PIECE_BUFFER_LENGTH 100
#define SIZE_OF_BOARD 24


typedef struct {
  int playerID;
  int pieceID;
  int position;
  bool isCaptured;
  bool isAvailable;
}Piece;

typedef struct {
  int moveTimeInMS;
  int captureAmount;
  int pieceAmount;
  Piece pieces[PIECE_BUFFER_LENGTH];
} Move;

typedef struct {
  bool isReady;
  int playerID;
  char playerName[STRING_BUFFER_LENGTH];
} Player;

typedef struct {
  Player players[PLAYER_BUFFER_LENGTH];
  Move currentMove;
  char gameName[STRING_BUFFER_LENGTH];
  int clientPlayerID;
  int playerAmount;
  int thinkerPID;
  int connecterPID;
  bool flag;
} SharedMemory;

void makeMove(bool player, int* board, int move[2]);
void undoMove(bool player, int* board, int move[2]);
int miniMax(bool player, int currentDepth, int depth, int* board, int* bestMove);
int generiereMoeglicheZuege(bool player, int* boardMiniMax, int b, int possibleMoves[][b]);
bool keineZuegeMehr(bool player, int* boardMiniMax, int b, int possibleMoves[][b]);
bool think(int fd [2], int n, int sharedMemoryID);
char* minus(char first, char second, int mode);
int initSocket(GameServerConfig *config);
void writePIDSharedMemory(int sharedMemoryID, int childPID);
int createSharedMemory(size_t size);
void createPipe(int fd[2]);
int createFork();
void printBoard(int sharedMemoryID);
void placePiece(char* move, int sharedMemoryID);
void movePiece(char* move);
int generateRandomNumber(int lower, int upper);
int evaluateBoard(int board[]);
int evaluatePiecesOnBoard(int board[], int value);
int evaluateMills(int board[], int value);