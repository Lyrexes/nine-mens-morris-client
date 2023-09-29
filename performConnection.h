#pragma once
#include "config.h"
#include "prolog.h"
#include <stdbool.h>
#include <stdlib.h>

#define MAX_MOVE_LENGTH 6

int receiveCurrentSequence(int socket, char *messageBuffer);
bool receiveGameOver(int socket, Move *move, char *messageBuffer, PrologResult prologResult);
bool receiveQUIT(int socket, char *messageBuffer);
bool receiveMOVEOK(int socket, char *messageBuffer);
int player0Won(int socket, char *messageBuffer);
int player1Won(int socket, char *messageBuffer);
bool sendOKWAIT(int socket, char *messageBuffer);
bool sendTHINKING(int socket, char *messageBuffer);
bool sendPLAY(int socket, char *messageBuffer, char *spielzug);
void receiveTIMEOUT(int socket, char *messageBuffer);
bool receiveOKTHINK(int socket, char *messageBuffer);
bool receiveMove(int socket, Move *move, char *messageBuffer);
int parseCaptureAmount(int socket, char* messageBuffer);
int parseMoveTime(int socket, char* messageBuffer);
int parsePieceAmount(int socket, char* messageBuffer);
bool parsePiece(int socket, char* messageBuffer, Piece* piece);
void debugPrintGameOver(Move *move);
void debugPrintMove(Move* move);


/* Converts string server position to int position
   Parameter
    position: string
  Return
    "A"  -> -1, isAvailable = true, isCaptured = false
    "C"  -> -1, isAvailable = false, isCaptured = true
    "AX" -> 0+X, isAvailable = false, isCaptured = false
    "BX" -> 8+X, isAvailable = false, isCaptured = false
    "CX" -> 16+X, isAvailable = false, isCaptured = false
  */
void parsePosition(char* position, Piece* piece);

bool performConnection(int socket, GameServerConfig *config, int sharedMemoryID, int fd [2]);
SharedMemory* attachSharedMemory(int sharedMemoryID);
void detachSharedMemory(SharedMemory* sharedMemoryPtr);
bool sendMessage(int socket, char *message);
void *safe_malloc(size_t n);
/* Reads exactly one Line from socket and writes it to bufferToWrite.
   Parameter
    socket: Socket file descriptor
    bufferToWrite: Buffer
    length: Maximum length of bufferToWrite
  Return
    true on success
    false if any error occured content of bufferToWrite is undefined
  */
bool receiveLine(int socket, char *bufferToWrite, int length);
void init_shared_memory(PrologResult* result, int sharedMemoryPtr);
int createEpoll();
bool closeEpoll(int epoll_fd);
