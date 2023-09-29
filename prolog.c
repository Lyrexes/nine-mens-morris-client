#include "prolog.h"
#include "performConnection.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CLIENT_VERSION "3.1"
#define MESSAGE_BUFFER_LENGTH 256

PrologResult prolog(int socket, GameServerConfig *config) {
  char messageBuffer[MESSAGE_BUFFER_LENGTH];

  PrologResult result;
  result.clientPlayerID = 0;
  result.playerAmount = 0;
  result.success = false;

  // MNM Gameserver accepting Connections
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH) ||
      !isExpectedPrologVersion(messageBuffer) ||
      !prettyPrintVersion(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH) ||
      !isExpectedPrologVar(messageBuffer) ||
      !prettyPrintVar(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH); 

  // Übertragen Client Version
  sprintf(messageBuffer, "VERSION %s\n", CLIENT_VERSION);
  if (!sendMessage(socket, messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  // Client version accepted - please send Game-ID to join
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH) ||
      !isExpectedPrologGameID(messageBuffer) ||
      !prettyPrintGameID(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  // Übertragen Game-ID
  sprintf(messageBuffer, "ID %s\n", config->gameID);
  if (!sendMessage(socket, messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  // Playing ...
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH) ||
      !isExpectedPrologGameKind(messageBuffer) ||
      !prettyPrintGameKind(messageBuffer)) {
    result.success = false;
    return result;
  }

  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);
  // Game Name
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH)) {
    result.success = false;
    return result;
  }
  if (!parseGameName(messageBuffer, result.gameName, STRING_BUFFER_LENGTH)) {
    printf("Clientfehler: Spielername ist zu groß max: %i",
           STRING_BUFFER_LENGTH);
    result.success = false;
    return result;
  }
  if (!isExpectedPrologGameName(messageBuffer) ||
      !prettyPrintGameName(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  // Übertragen PLAYER
  if(config->playerNumber == -1) {      // kein Spielerwunsch
    sprintf(messageBuffer, "PLAYER\n");
  }else {                               // gewünschte Spielernummer mitschicken
    sprintf(messageBuffer, "PLAYER %d\n", config->playerNumber);
  }
  if (!sendMessage(socket, messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  // Erhalten Spielerliste
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH)) {
    result.success = false;
    return result;
  }
  result.clientPlayerID = parsePlayerAmount(messageBuffer);
  if (!isExpectedPrologClient(messageBuffer) ||
      !prettyPrintClient(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH)) {
    result.success = false;
    return result;
  }
  result.playerAmount = parsePlayerAmount(messageBuffer);
  if (result.playerAmount > PLAYER_BUFFER_LENGTH) {
    result.success = false;
    return result;
  }
  if (!isExpectedPrologPlayerAmount(messageBuffer) ||
      !prettyPrintPlayerAmount(messageBuffer)) {
    result.success = false;
    return result;
  }
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);

  result.success = receivePlayerReady(socket, messageBuffer, &result);
  return result;
}

bool prettyPrintVersion(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  char *version = NULL;
  char *start = NULL;
  int length = 0;
  bool isReadingVersion = false;
  char currChar = 0;
  for (int i = 0; i < strlen(serverMessage); i++) {
    currChar = serverMessage[i];
    if (isdigit(currChar) && !isReadingVersion) {
      start = serverMessage + i;
      isReadingVersion = true;
    }
    if (isReadingVersion) {
      if (!isdigit(currChar) && currChar != '.')
        break;
      length++;
    }
  }
  version = safe_malloc(length * sizeof(char) + 1);
  version[length] = '\0';
  strncpy(version, start, length);
  printf("Der MNM Gamerserver mit der Version %s akzeptiert Verbindungen\n",
         version);
  free(version);
  return true;
}

bool prettyPrintVar(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  printf("Der Server will wissen, ob du schon zufrieden bist mit der AI\n");
  return true;
}

bool prettyPrintGameID(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  printf("Die Client Version wurde akzeptiert\n");
  return true;
}

bool prettyPrintGameKind(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  char *gameKind = NULL;
  char *start = NULL;
  int length = 0;
  bool isReadingGameKind = false;
  char currChar = 0;
  int spaceCount = 0;
  for (int i = 0; i < strlen(serverMessage); i++) {
    currChar = serverMessage[i];
    if (spaceCount == 2 && !isReadingGameKind) {
      start = serverMessage + i;
      isReadingGameKind = true;
    }
    if (isReadingGameKind) {
      if (currChar == '\n')
        break;
      length++;
    }
    if (currChar == ' ')
      spaceCount++;
  }
  gameKind = safe_malloc(length * sizeof(char) + 1);
  gameKind[length] = '\0';
  strncpy(gameKind, start, length);

  if (strcmp(gameKind, "NMMorris") != 0) {
    printf("Fehler: Es wird nicht NMMorris gespielt!\n");
    free(gameKind);
    return false;
  }

  printf("Es wird %s gespielt\n", gameKind);
  free(gameKind);
  return true;
}

bool prettyPrintClient(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  char *clientName = safe_malloc(strlen(serverMessage) * sizeof(char));
  int clientNumber = 0;
  char *currToken = NULL;
  int wordIndex = 0;
  currToken = strtok(serverMessage, " \n");
  while (currToken != NULL) {
    if (wordIndex == 2) {
      clientNumber = atoi(currToken);
    } else if (wordIndex == 3) {
      strcpy(clientName, currToken);
    } else if (wordIndex > 3) {
      strcat(clientName, " ");
      strcat(clientName, currToken);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  printf("Cient-Spielername ist \"%s\", mit Nummer: (%i)\n", clientName,
         clientNumber + 1);
  free(clientName);
  return true;
}

bool prettyPrintPlayerAmount(char *serverMessage) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  int playerAmount = parsePlayerAmount(serverMessage);
  printf("Spieleranzal ist (%i)\n", playerAmount);
  return true;
}

void prettyPrintError(char *serverMessage) {
  // check for substring
  if (strstr(serverMessage, "Did not get the expected") != NULL &&
      strstr(serverMessage, "command") != NULL)
    printf("Unerwarteter Befehl!\n");
  else if (strstr(serverMessage, "Version does not match") != NULL)
    printf("Die Versionen von Client und Server sind inkompatibel.\n");
  else if (strstr(serverMessage, "Game does not exist") != NULL)
    printf("Es existiert kein Spiel mit dieser Game-ID.\n");
  else if (strstr(serverMessage, "No free player") != NULL)
    printf("Es gibt keinen verfügbaren Spieler.\n");
  else if (strstr(serverMessage, "TIMEOUT") != NULL)
    printf("Zeitüberschreitung!\n");
  else if (strstr(serverMessage, "Internal error") != NULL)
    printf("Fehler beim Server!\n");
  else if (strstr(serverMessage, "Invalid Move: Destination is already occupied") != NULL)
    printf("Ungültiger Zug. Der übermittelte Platz ist bereits durch einen anderen Stein belegt.\n");
  else
    printf("Unbekannter Fehler: %s", serverMessage + 2);
}

bool receivePlayerReady(int socket, char *messageBuffer, PrologResult *result) {
  bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);
  if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH))
    return false;
  Player currentPlayer;
  int playerIndex = 0;
  while (strcmp(messageBuffer, "+ ENDPLAYERS\n") != 0) {
    if (!isExpectedPrologPlayer(messageBuffer))
      return false;
    if (!parsePlayer(messageBuffer, &currentPlayer)) {
      printf("Clientfehler: Spielername ist zu lang, max: %i",
             STRING_BUFFER_LENGTH);
      return false;
    }
    prettyPrintPlayer(messageBuffer, &currentPlayer);
    bzero(messageBuffer, MESSAGE_BUFFER_LENGTH);
    if (!receiveLine(socket, messageBuffer, MESSAGE_BUFFER_LENGTH))
      return false;
    result->players[playerIndex] = currentPlayer;
  }
  return true;
}

bool prettyPrintGameName(char *serverMessage) {
  char gameName[STRING_BUFFER_LENGTH];
  if (!parseGameName(serverMessage, gameName, STRING_BUFFER_LENGTH)) {
    return false;
  }
  printf("Das Spiel hat den Namen \"%s\"\n", gameName);
  return true;
}

bool prettyPrintPlayer(char *serverMessage, Player *player) {
  if (serverMessage[0] == '-') {
    prettyPrintError(serverMessage);
    return false;
  }
  printf("Spieler \"%s\", mit Nummer: (%i) ist%sbereit\n", player->playerName,
         player->playerID + 1, player->isReady ? " " : " nicht ");  // KARO: Spieler hochzählen?
  return true;
}

bool messageEqualsWithout(char *serverMessage, char **msgs, int msgsLength,
                          int without) {
  char *message = safe_malloc(strlen(serverMessage) * sizeof(char) + 1);
  strcpy(message, serverMessage);
  char *currToken = NULL;
  int wordIndex = 0;
  currToken = strtok(message, " \n");
  while (currToken != NULL) {
    if (wordIndex == without) {
      currToken = strtok(NULL, " \n");
    }
    if (wordIndex >= msgsLength || strcmp(currToken, msgs[wordIndex]) != 0) {
      free(message);
      return false;
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  free(message);
  return true;
}

bool messageStartsWith(char *message, char **check, int checkLength) {
  char *msg = safe_malloc(strlen(message) * sizeof(char) + 1);
  strcpy(msg, message);
  char *currToken = NULL;
  int wordIndex = 0;
  currToken = strtok(msg, " \n");
  while (currToken != NULL) {
    if (wordIndex >= checkLength) {
      free(msg);
      return true;
    }
    if (strcmp(currToken, check[wordIndex]) != 0) {
      free(msg);
      return false;
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  free(msg);
  return wordIndex == checkLength;
}

void printExpected(char *serverMessage, char **expectedArr, int length) {
  printf("Protokollfehler: Unerwartete Server Nachricht:\n"
         "\terhalten: %s\terwartet: ",
         serverMessage);
  for (int i = 0; i < length; i++) {
    printf("%s ", expectedArr[i]);
  }
  printf("\n");
}

bool isExpectedPrologClient(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[2] = {"+", "YOU"};
  if (!messageStartsWith(serverMessage, expected, 2)) {
    printExpected(serverMessage, expected, 2);
    return false;
  }
  return true;
}

bool isExpectedPrologVersion(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[5] = {"+", "MNM", "Gameserver", "accepting", "connections"};
  if (!messageEqualsWithout(serverMessage, expected, 5, 3)) {
    printExpected(serverMessage, expected, 5);
    return false;
  }
  return true;
}

bool isExpectedPrologVar(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[6] = {"+", "Already", "happy", "with", "your", "AI?"};
  if (!messageStartsWith(serverMessage, expected, 6)) {
    printExpected(serverMessage, expected, 6);
    return false;
  }
  return true;
}

bool isExpectedPrologGameID(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[10] = {"+",      "Client", "version", "accepted", "-",
                        "please", "send",   "Game-ID", "to",       "join"};
  if (!messageStartsWith(serverMessage, expected, 10)) {
    printExpected(serverMessage, expected, 10);
    return false;
  }
  return true;
}

bool isExpectedPrologGameKind(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[2] = {"+", "PLAYING"};
  if (!messageStartsWith(serverMessage, expected, 2)) {
    printExpected(serverMessage, expected, 2);
    return false;
  }
  return true;
}

bool isExpectedPrologGameName(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[1] = {"+"};
  if (!messageStartsWith(serverMessage, expected, 1)) {
    printExpected(serverMessage, expected, 1);
    return false;
  }
  return true;
}

bool isExpectedPrologPlayer(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[1] = {"+"};
  if (!messageStartsWith(serverMessage, expected, 1)) {
    printExpected(serverMessage, expected, 1);
    return false;
  }
  return true;
}

bool isExpectedPrologPlayerAmount(char *serverMessage) {
  if (serverMessage[0] == '-')
    return true;
  char *expected[2] = {"+", "TOTAL"};
  if (!messageStartsWith(serverMessage, expected, 2)) {
    printExpected(serverMessage, expected, 2);
    return false;
  }
  return true;
}

bool parseGameName(char *serverMessage, char *gameName, int maxLength) {
  char *start = NULL;
  int length = 0;
  bool isReadingGameName = false;
  char currChar = 0;
  int spaceCount = 0;
  for (int i = 0; i < strlen(serverMessage); i++) {
    currChar = serverMessage[i];
    if (spaceCount == 1 && !isReadingGameName) {
      start = serverMessage + i;
      isReadingGameName = true;
    }
    if (isReadingGameName) {
      if (currChar == '\n')
        break;
      length++;
    }
    if (currChar == ' ')
      spaceCount++;
  }
  if (length + 1 > maxLength) {
    return false;
  }
  strncpy(gameName, start, length);
  gameName[length] = '\0';
  return true;
}

int parsePlayerAmount(char *serverMessage) {
  char *msg = safe_malloc(strlen(serverMessage) * sizeof(char) + 1);
  strcpy(msg, serverMessage);
  char *currToken = NULL;
  int wordIndex = 0;
  int amount = 0;
  currToken = strtok(msg, " \n");
  while (currToken != NULL) {
    if (wordIndex == 2) {
      amount = atoi(currToken);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  free(msg);
  return amount;
}

int parseClienPlayerID(char *serverMessage) {
  char *msg = safe_malloc(strlen(serverMessage) * sizeof(char) + 1);
  strcpy(msg, serverMessage);
  char *currToken = NULL;
  currToken = strtok(msg, " \n");
  int wordIndex = 0;
  int id = 0;
  while (currToken != NULL) {
    if (wordIndex == 2) {
      id = atoi(currToken);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  free(msg);
  return id;
}

bool parsePlayer(char *serverMessage, Player *player) {
  char playerNumberString[3];
  char playerName[STRING_BUFFER_LENGTH];
  // count spaces
  int count = 0;
  for (int i = 2; i < (int)strlen(serverMessage); i++) {
    if (serverMessage[i] == ' ')
      count++;
  }
  int currentLetter = 2; // to ignore "+ "
  // Read Player Number
  int playerNumberIndex = 0;
  while (serverMessage[currentLetter] != ' ') {
    playerNumberString[playerNumberIndex] = serverMessage[currentLetter];
    playerNumberIndex++;
    currentLetter++;
  }
  playerNumberString[playerNumberIndex] = '\0';
  int playerNumber = atoi(playerNumberString);
  currentLetter++;
  int playerNumberStringLength = playerNumberIndex;
  int playerNameLength = STRING_BUFFER_LENGTH - (5 + playerNumberStringLength);
  if (playerNameLength >= STRING_BUFFER_LENGTH) {
    printf("Clientfehler: Spielername ist zu lang mit Nummer: %i",
           playerNumber);
    return false;
  }
  int spacesInName = count - 2;
  // Read Player Name
  int playerNameIndex = 0;

  while (spacesInName >= 0) {
    playerName[playerNameIndex] = serverMessage[currentLetter];
    playerNameIndex++;
    currentLetter++;
    if (serverMessage[currentLetter] == ' ')
      spacesInName--;
  }
  playerName[playerNameIndex] = '\0';
  currentLetter++;

  // Read Ready or Not
  player->isReady = serverMessage[currentLetter] == '1';
  player->playerID = playerNumber;
  strcpy(player->playerName, playerName);
  return true;
}
