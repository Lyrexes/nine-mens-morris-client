#include "performConnection.h"
#include "prolog.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h> 

#define MAXEVENTS 64

bool performConnection(int socket, GameServerConfig *config, int sharedMemoryID, int fd [2]) {

  //Prolog
  PrologResult prologResult = prolog(socket, config);
  if (!prologResult.success) {
    return false;
  } else {
    printf("----- Prolog erfolgreich abgeschlossen ------\n");
  }
  init_shared_memory(&prologResult, sharedMemoryID);

  //Protokollphase Spielverlauf
  bool gameover = false;
  char messageBuffer[STRING_BUFFER_LENGTH];
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  do{
    int currentSequence = -1;
    currentSequence = receiveCurrentSequence(socket, messageBuffer);
    //Fehler
    if(currentSequence == -1){
      return false;
    }
    //Idle Befehlssequenz (Wait)
    if(currentSequence == 0){
      printf("WAIT vom server\n");
      if(!sendOKWAIT(socket, messageBuffer)) return false;

    }
    //Move Befehlssequenz
    if(currentSequence == 1){
      printf("MOVE vom server\n");
      SharedMemory* sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
      if (!receiveMove(socket, &sharedMemoryPtr->currentMove, messageBuffer)) {
        return false;
      }
      //detachSharedMemory(sharedMemoryPtr);
      if(!sendTHINKING(socket, messageBuffer)) return false;
      debugPrintMove(&sharedMemoryPtr->currentMove);
      if(!receiveOKTHINK(socket, messageBuffer)) return false;
      else printf("Server erwartet einen Spielzug.\n");

      //epoll
      int epoll_fd = -1, temp = -1;
      struct epoll_event event = {.events = EPOLLIN | EPOLLET, .data.fd = fd[0]};
      struct epoll_event *events;
      epoll_fd = createEpoll();

      //Signal an Thinker schicken
      sharedMemoryPtr->flag = true;   // Flag setzen
      if(kill(getppid(), SIGUSR1) != 0) {
        printf("Fehler beim Senden des Signals SIGUSR1 von Connector an Thinker!\n");
        exit(EXIT_FAILURE);
      }                               // Signal senden

      temp = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd[0], &event);
      if(temp == -1){
        printf("Fehler bei epoll_ctl: Konnte fd[0] nicht hinzufügen\n");
        return false;
      }
      event.data.fd = socket;
      event.events = EPOLLIN | EPOLLET;
      temp = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket, &event);
      if(temp == -1){
        printf("Fehler bei epoll_clt: Konnte socket nicht hinzufügen\n");
        return false;
      }
      events = calloc (MAXEVENTS, sizeof event);
      bool received = false;
      while(!received){
        int n, i;
        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
        for(i = 0; i < n; i++){
          if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))){
            printf ("Fehler bei epoll.\n");
            free (events);
            if(!closeEpoll(epoll_fd)) return false;
            return false;
          } else if (fd[0] == events[i].data.fd){
            char move[MAX_MOVE_LENGTH];
            read (fd[0], move, sizeof(move));
            move[MAX_MOVE_LENGTH-1] = '\0';
            if(!sendPLAY(socket, messageBuffer, move)) return false;
            received = true;
          } else if (socket == events[i].data.fd){
            receiveTIMEOUT(socket, messageBuffer);
            free (events);
            if(!closeEpoll(epoll_fd)) return false;
            return false;
          }
        }
      }
      detachSharedMemory(sharedMemoryPtr);
      free (events);
      if(!closeEpoll(epoll_fd)) return false; //epoll instanz löschen
      if(!receiveMOVEOK(socket, messageBuffer)) return false;
      else printf("Spielzug erfolgreich an den Server geschickt.\n");
    }
    //gameover
    if(currentSequence == 2){
      printf("GAMEOVER vom server\n");
      gameover = true;
      SharedMemory* sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
      if (!receiveGameOver(socket, &sharedMemoryPtr->currentMove, messageBuffer, prologResult)) {
        return false;
      }
      debugPrintGameOver(&sharedMemoryPtr->currentMove);
      detachSharedMemory(sharedMemoryPtr);
    }
  } while(gameover == false);
  return true;
}

bool sendOKWAIT(int socket, char *messageBuffer){
  sprintf(messageBuffer, "OKWAIT\n");
  if (!sendMessage(socket, messageBuffer)) {
    return false;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return true;
}

bool sendTHINKING(int socket, char *messageBuffer){
  sprintf(messageBuffer, "THINKING\n");
  if (!sendMessage(socket, messageBuffer)) {
    return false;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return true;
}

bool sendPLAY(int socket, char *messageBuffer, char *spielzug){
  sprintf(messageBuffer, "PLAY %s\n", spielzug);
  if (!sendMessage(socket, messageBuffer)) {
    return false;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return true;
}

void receiveTIMEOUT(int socket, char *messageBuffer){
  if (receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    } else if (messageBuffer[0] == '+') {
    printf("Unerwartete Nachricht vom Server erhalten: %s\n", messageBuffer);
    }
  } else {
    printf("Konnte Servernachricht nicht empfangen.\n");
  }
}

bool receiveOKTHINK(int socket, char *messageBuffer){
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return false;
  }
   if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return false;
  }
  char *expected[2] = {"+", "OKTHINK"};
  if (!messageStartsWith(messageBuffer, expected, 2)) {
    printExpected(messageBuffer, expected, 2);
    return false;
  }
  return true;
}

bool receiveQUIT(int socket, char *messageBuffer){
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return false;
  }
   if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return false;
  }
  char *expected[2] = {"+", "QUIT"};
  if (!messageStartsWith(messageBuffer, expected, 2)) {
    printExpected(messageBuffer, expected, 2);
    return false;
  }
  return true;
}

bool receiveMOVEOK(int socket, char *messageBuffer){
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return false;
  }
   if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return false;
  }
  char *expected[2] = {"+", "MOVEOK"};
  if (!messageStartsWith(messageBuffer, expected, 2)) {
    printExpected(messageBuffer, expected, 2);
    return false;
  }
  return true;
}

int receiveCurrentSequence(int socket, char *messageBuffer){
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return -1;
  }
  if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return -1;
  }
  char *expectedWait[2] = {"+", "WAIT"};
  if (messageStartsWith(messageBuffer, expectedWait, 2)) {
    return 0;
  }
  char *expectedMove[2] = {"+", "MOVE"};
  if (messageStartsWith(messageBuffer, expectedMove, 2)) {
    return 1;
  }
  char *expectedGameOver[2] = {"+", "GAMEOVER"};
  if (messageStartsWith(messageBuffer, expectedGameOver, 2)) {
    return 2;
  }
  printf("Keine der erwarteten Nachrichten (WAIT, MOVE, GAMEOVER) vom Server erhalten.\n");
  return -1;
} 

bool receiveGameOver(int socket, Move *move, char *messageBuffer, PrologResult prologResult){
  int result = 0;
  if ((result = parsePieceAmount(socket, messageBuffer)) == -1) {
      return false;
    }
    move->pieceAmount = result;
    if (move->pieceAmount > PIECE_BUFFER_LENGTH) {
      printf("Clientfehler: Game has to many pieces, max: %i",
            PIECE_BUFFER_LENGTH);
      return false;
    }

    int pieceIndex = 0;
    while (strcmp(messageBuffer, "+ ENDPIECELIST\n") != 0) {
      if (!parsePiece(socket, messageBuffer, &move->pieces[pieceIndex])) {
        return false;
      }
      pieceIndex++;
    }

    //+PLAYER0WON
    bool player0 = false;
    int temp = -1;
    temp = player0Won(socket, messageBuffer);
    if (temp == -1) return false;
    else if (temp == 0) player0 = false;
    else if (temp == 1) player0 = true;
    
    //+PLAYER1WON
    bool player1= false;
    temp = -1;
    temp = player1Won(socket, messageBuffer);
    if (temp == -1) return false;
    else if (temp == 0) player1 = false;
    else if (temp == 1) player1 = true;

    if(player0 == true && player1 == true) printf("Das Spiel endete mit einem Unentschieden.\n");
    else if (player0 == true){
        if(prologResult.clientPlayerID == 0) printf("Du hast gewonnen. Herzlichen Glückwunsch!\n");
        else printf("Spieler 1 hat gewonnen.\n");
    } else if (player1== true){
        if(prologResult.clientPlayerID == 1) printf("Du hast gewonnen. Herzlichen Glückwunsch!\n");
        else printf("Spieler 2 hat gewonnen.\n");
    }

    //+QUIT
    if(!receiveQUIT(socket, messageBuffer)) return false;
    else printf("QUIT vom Server erhalten.\n");

    return true;
}

int player0Won(int socket, char *messageBuffer){ //-1 bei Fehler, 0 bei Verloren, 1 bei Gewonnen
    if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
        return -1;
    }
    if (messageBuffer[0] == '-') {
        prettyPrintError(messageBuffer);
        return -1;
    }

    char *expectedWon[3] = {"+", "PLAYER0WON", "Yes"};
    char *expectedLose[3] = {"+", "PLAYER0WON", "No"};
    if (messageStartsWith(messageBuffer, expectedLose, 3)) {
         return 0;
    } else if (messageStartsWith(messageBuffer, expectedWon, 3)){
        return 1;
    } else {
        printf("Protokollfehler: Unerwartete server Nachricht. Erwartet: PLAYER0WON Yes oder PLAYER0WON No. Eerhalten: %s", messageBuffer);
        return -1;
    }
    return -1;
}

int player1Won(int socket, char *messageBuffer){ //-1 bei Fehler, 0 bei Verloren, 1 bei Gewonnen
    if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
        return -1;
    }
    if (messageBuffer[0] == '-') {
        prettyPrintError(messageBuffer);
        return -1;
    }

    char *expectedWon[3] = {"+", "PLAYER1WON", "Yes"};
    char *expectedLose[3] = {"+", "PLAYER1WON", "No"};
    if (messageStartsWith(messageBuffer, expectedLose, 3)) {
         return 0;
    } else if (messageStartsWith(messageBuffer, expectedWon, 3)){
        return 1;
    } else {
        printf("Protokollfehler: Unerwartete Server Nachricht. Erwartet: PLAYER1WON Yes oder PLAYER1WON No. Erhalten: %s", messageBuffer);
        return -1;
    }
    return -1;
}

bool receiveMove(int socket, Move *move, char *messageBuffer) {
  int result = 0;

  if ((result = parseMoveTime(socket, messageBuffer)) == -1) {
    return false;
  }
  move->moveTimeInMS = result;

  if ((result = parseCaptureAmount(socket, messageBuffer)) == -1) {
    return false;
  }
  move->captureAmount = result;

  if ((result = parsePieceAmount(socket, messageBuffer)) == -1) {
    return false;
  }
  move->pieceAmount = result;
  if (move->pieceAmount > PIECE_BUFFER_LENGTH) {
    printf("Clientfehler: Game has to many pieces, max: %i",
           PIECE_BUFFER_LENGTH);
    return false;
  }

  int pieceIndex = 0;
  while (strcmp(messageBuffer, "+ ENDPIECELIST\n") != 0) {
    if (!parsePiece(socket, messageBuffer, &move->pieces[pieceIndex])) {
      return false;
    }
    pieceIndex++;
  }
  return true;
}

void debugPrintGameOver(Move *move){
  printf("pieceAmount: %i\n", move->pieceAmount);
    for (int i = 0; i < move->pieceAmount; i++) {
      printf("piece: playerID: %i, pieceID: %i, isCaptured: %i, isAvailable: %i, "
            "position: %i\n",
            move->pieces[i].playerID, move->pieces[i].pieceID,
            move->pieces[i].isCaptured, move->pieces[i].isAvailable,
            move->pieces[i].position);
    }
}

void debugPrintMove(Move *move) {
  printf("moveTime: %i\n", move->moveTimeInMS);
  printf("captureAmount: %i\n", move->captureAmount);
  printf("pieceAmount: %i\n", move->pieceAmount);
  for (int i = 0; i < move->pieceAmount; i++) {
    printf("piece: playerID: %i, pieceID: %i, isCaptured: %i, isAvailable: %i, "
           "position: %i\n",
           move->pieces[i].playerID, move->pieces[i].pieceID,
           move->pieces[i].isCaptured, move->pieces[i].isAvailable,
           move->pieces[i].position);
  }
}
int parseMoveTime(int socket, char *messageBuffer) {
  int moveTime = -1;
  char *currToken = NULL;
  int wordIndex = 0;
  currToken = strtok(messageBuffer, " \n");
  while (currToken != NULL) {
    if (wordIndex == 2) {
      moveTime = atoi(currToken);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return moveTime;
}

int parseCaptureAmount(int socket, char *messageBuffer) {
  int captureAmount = -1;
  char *currToken = NULL;
  int wordIndex = 0;
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return -1;
  }
  if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return -1;
  }
  currToken = strtok(messageBuffer, " \n");
  while (currToken != NULL) {
    if (wordIndex == 2) {
      captureAmount = atoi(currToken);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return captureAmount;
}

int parsePieceAmount(int socket, char *messageBuffer) {
  int pieceAmount = -1;
  char *currToken = NULL;
  char *secondNumberOffset = NULL;
  int wordIndex = 0;
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return -1;
  }
  if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return -1;
  }
  currToken = strtok(messageBuffer, " \n");
  while (currToken != NULL) {
    if (wordIndex == 2) {
      pieceAmount = strtol(currToken, &secondNumberOffset, 10);
      secondNumberOffset++;
      pieceAmount *= strtol(secondNumberOffset, NULL, 10);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  return pieceAmount;
}

bool parsePiece(int socket, char *messageBuffer, Piece *piece) {
  bzero(messageBuffer, STRING_BUFFER_LENGTH);
  char *currToken = NULL;
  char *pointSeperator = NULL;
  int wordIndex = 0;
  if (!receiveLine(socket, messageBuffer, STRING_BUFFER_LENGTH)) {
    return false;
  }
  if (messageBuffer[0] == '-') {
    prettyPrintError(messageBuffer);
    return false;
  }
  if (strcmp(messageBuffer, "+ ENDPIECELIST\n") == 0) {
    return true;
  }
  currToken = strtok(messageBuffer, " \n");
  while (currToken != NULL) {
    if (wordIndex == 1) {
      pointSeperator = strchr(currToken, '.');
      piece->playerID = atoi(pointSeperator - 1);
      piece->pieceID = atoi(pointSeperator + 1);
    } else if (wordIndex == 2) {
      parsePosition(currToken, piece);
    }
    currToken = strtok(NULL, " \n");
    wordIndex++;
  }
  return true;
}

void parsePosition(char *position, Piece *piece) {
  int offsetFactor = position[0] - 'A';
  piece->isCaptured = false;
  piece->isAvailable = false;
  if (strlen(position) == 1) {
    piece->position = -1;
    if (offsetFactor == 0) {
      piece->isAvailable = true;
    } else {
      piece->isCaptured = true;
    }
    return;
  }
  piece->position = offsetFactor * 8 + atoi(position + 1);
}

bool receiveLine(int socket, char *bufferToWrite, int length) {
  char *currMessagePtr = bufferToWrite;
  int currBufferSize = length - 1;
  int receiveResult = 0;
  while (currBufferSize > 0) {
    receiveResult = read(socket, currMessagePtr, 1);
    if (*currMessagePtr == '\n') {
      *(currMessagePtr + 1) = '\0';
      return true;
    } else if (receiveResult < 0) {
      perror("Fehler beim empfangen");
      return false;
    }
    currMessagePtr++;
    currBufferSize--;
  }
  return false;
}

bool sendMessage(int socket, char *message) {
  char *currMessagePtr = message;
  int msgLengthToSend = strlen(message);
  int bytesSend = 0;
  while (msgLengthToSend > 0) {
    bytesSend = send(socket, currMessagePtr, msgLengthToSend, 0);
    if (bytesSend == -1) {
      perror("Fehler beim senden");
      return false;
    }
    currMessagePtr += bytesSend;
    msgLengthToSend -= bytesSend;
  }
  return true;
}

void *safe_malloc(size_t n) {
  void *pointer = malloc(n);
  if (!pointer) {
    fprintf(stderr, "Out of memory(%lu bytes)\n", (unsigned long)n);
    exit(EXIT_FAILURE);
  }
  memset(pointer, 0, n);
  return pointer;
}

void init_shared_memory(PrologResult *result, int sharedMemoryID) {
  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  sharedMemoryPtr->clientPlayerID = result->clientPlayerID;
  sharedMemoryPtr->playerAmount = result->playerAmount;
  strcpy(sharedMemoryPtr->gameName, result->gameName);
  for (int i = 0; i < result->playerAmount; i++) {
    sharedMemoryPtr->players[i] = result->players[i];
  }
  detachSharedMemory(sharedMemoryPtr);
}

SharedMemory *attachSharedMemory(int sharedMemoryID) {
  void *sharedMemoryPtr = shmat(sharedMemoryID, 0, 0);
  if (sharedMemoryPtr == NULL) {
    printf("clientfehler: sharedMemoryAttach failed");
    exit(EXIT_FAILURE);
  }
  return (SharedMemory *)sharedMemoryPtr;
}

void detachSharedMemory(SharedMemory *sharedMemoryPtr) {
  int detachResult = shmdt(sharedMemoryPtr);
  if (detachResult == -1) {
    printf("clientfehler: sharedMemorydetach failed");
    exit(EXIT_FAILURE);
  }
}

int createEpoll(){
  int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		printf("Fehler beim Erstellen des epoll file descriptors.\n");
		exit(EXIT_FAILURE);
	}
  return epoll_fd;
}

bool closeEpoll(int epoll_fd){
  if (close(epoll_fd)) {
		printf("Fehler beim Schließen des epoll file descriptors.\n");
		return false;
	}
  return true;
}
