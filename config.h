#pragma once
#include <stdbool.h>
#include <stdio.h>

typedef struct {
  char gameKindName[256];
  char hostname[256];
  char* gameID;
  unsigned int playerNumber;
  unsigned int portNumber;
}GameServerConfig;

GameServerConfig parseArguments(int argc, char* argv[]);
bool removeBlankSpaces(char* filename);
char *readValueAsString(char *configfile, char const *desired_name);
