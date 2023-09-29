#pragma once
#include "config.h"
#include "sysprak-client.h"

typedef struct {
  Player players[PLAYER_BUFFER_LENGTH];
  char gameName[STRING_BUFFER_LENGTH];
  int clientPlayerID;
  int playerAmount;
  bool success;
} PrologResult;

bool parsePlayer(char* serverMessage, Player* player);
bool parseGameName(char *serverMessage, char *gameName, int maxLength);
int parsePlayerAmount(char *serverMessage);
int parseClienPlayerID(char *serverMessage);
PrologResult prolog(int socket, GameServerConfig *config);
bool receivePlayerReady(int socket, char *serverMessage, PrologResult *result);
bool prettyPrintClient(char *serverMessage);
bool prettyPrintVersion(char *serverMessage);
bool prettyPrintVar(char *serverMessage);
bool prettyPrintGameID(char *serverMessage);
bool prettyPrintGameKind(char *serverMessage);
bool prettyPrintGameName(char *serverMessage);
bool prettyPrintPlayer(char *serverMessage, Player* player);
void prettyPrintError(char *serverMessage);
bool prettyPrintPlayerAmount(char *serverMessage);
bool messageStartsWith(char *message, char **check, int msgsLength);
bool messageEqualsWithout(char *serverMessage, char **msgs, int msgsLength,
                          int without);
bool isExpectedPrologClient(char *serverMessage);
bool isExpectedPrologVersion(char *serverMessage);
bool isExpectedPrologVar(char *serverMessage);
bool isExpectedPrologGameID(char *serverMessage);
bool isExpectedPrologGameKind(char *serverMessage);
bool isExpectedPrologPlayer(char *serverMessage);
bool isExpectedPrologPlayerAmount(char *serverMessage);
bool isExpectedPrologGameName(char *serverMessage);
void printExpected(char *message, char **expectedArr, int length);
