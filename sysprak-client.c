#include "sysprak-client.h"
#include "config.h"
#include "performConnection.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <float.h> //delete

int countMoves = 0;
double max_time = DBL_MIN;

const char boardPositions[24][MAX_MOVE_LENGTH] = {
    "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "B0", "B1", "B2", "B3",
    "B4", "B5", "B6", "B7", "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7"};

int sharedMemoryID;
const int n = MAX_MOVE_LENGTH;
int fd[2];
// Signalhandler
void handle_sigusr1(int sig) {
  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  /*
    check if flag is set (to make sure SIGUSR1 was sent from Connector)
    - if yes: reset flag & call think() function
    - if no: do nothing
  */
  if (sharedMemoryPtr->flag) {
    sharedMemoryPtr->flag = false;
    if (!think(fd, n, sharedMemoryID)) {
      printf("In der think-Methode ist etwas schiefgelaufen!\n");
      detachSharedMemory(sharedMemoryPtr);
      exit(EXIT_FAILURE);
    }
  } else {
    printf("Der Thinker hat ein SIGUSR1 erhalten, aber die Flag war nicht "
           "gesetzt.\n");
  }
  detachSharedMemory(sharedMemoryPtr);
}

int main(int argc, char *argv[]) {
  // für generateRandomNumber
  srand(time(0));

  // Kommandozeilenparameter einlesen
  GameServerConfig config = parseArguments(argc, argv);

  // Einrichten des Shared Memory bereich
  // zur Kommunikation zwischen Thinker und Connector
  sharedMemoryID = createSharedMemory(sizeof(SharedMemory));

  // Einrichten Pipe zur Kommunikation zwischen Thinker und Connector
  createPipe(fd);

  // Aufspaltung in Thinker und Connector
  pid_t pid = createFork();
  if (pid == 0) {

    // Connector (Kindprozess)

    close(fd[1]); // Schreibseite schließen

    // Serververbindung vorbereiten
    int socket = initSocket(&config);

    if (!performConnection(socket, &config, sharedMemoryID, fd)) {
      printf("PerformConnection war nicht erfolgreich!\n");
      exit(EXIT_FAILURE);
    }

    close(fd[0]); // Leseseite schließen

  } else if (pid > 0) {
    // Thinker (Elternprozess)
    struct sigaction sa = {0};
    sa.sa_handler = &handle_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);

    writePIDSharedMemory(sharedMemoryID, pid);

    close(fd[0]); // Leseseite schließen

    // Warten auf den Kindprozess
    int status;
    do {
      if ((pid = waitpid(pid, &status, WNOHANG)) == -1) {
        printf("Fehler beim Warten auf den Kindprozess.\n");
        exit(EXIT_FAILURE);
      } else if (pid == 0) { // Connector arbeitet noch
        sleep(1);
      } else {
        if (WIFEXITED(status)) {
          if (WEXITSTATUS(status) == EXIT_FAILURE) {
            printf("Connector wurde nicht erfolgreich beendet.\n");
            exit(EXIT_FAILURE);
          } else {
            printf("Connector wurde erfolgreich beendet.\n");
            exit(EXIT_SUCCESS);
          }
        }
      }
    } while (pid == 0);

    close(fd[1]); // Schreibseite schließen
    printf("Client wurde erfolgreich beendet.\n");
    exit(EXIT_SUCCESS);
  }
}

int miniMax(bool player, int currentDepth, int depth, int* board, int* bestMove) {
  int possibleMoves [64][2]; 
  if(currentDepth == 0 || keineZuegeMehr(player, board, 2, possibleMoves)){ 
    return evaluateBoard(board);
  }
  int maxValue = INT_MIN;
  int movesCount = 0;
  movesCount = generiereMoeglicheZuege(player, board, 2, possibleMoves); 
  for(int i = 0; i < movesCount; i++) {
    makeMove(player, board, possibleMoves[i]); 
    int value = -miniMax(!player, currentDepth-1, depth, board, bestMove);
    undoMove(player, board, possibleMoves[i]); 
    if(value > maxValue) {
      maxValue = value;
      if(currentDepth == depth) {
        bestMove[0] = possibleMoves[i][0];
        bestMove[1] = possibleMoves[i][1];
      }
    }
  }
  return maxValue;
}

int evaluateBoard(int board[]) {
  return evaluatePiecesOnBoard(board, 1) + evaluateMills(board, 1);
}

// +1 for Clients pieces, -1 for Opponents Pieces
int evaluatePiecesOnBoard(int board[], int value) {
  int result = 0;
  int piecesClient = 0;
  int piecesOpponent = 0;
  for(int i = 0; i < 24; i++) {
    if(board[i] == 1) {
      result += value;
      piecesClient++;
    }else if(board[i] == -1) {
      result -= value;
      piecesOpponent++;
    }
  }
  //printf("Der Client hat %d und der Gegner %d Steine auf dem Spielbrett.\n", piecesClient, piecesOpponent);
  return result;
}

// +1 for Clients Mills, -1 for Opponents Mills
int evaluateMills(int board[], int value) {
  int result = 0;
  int millsClient = 0;
  int millsOpponent = 0;
  // A
  int x = 0;
  int y = 1;
  int z = 2;
  for(int i = 0; i < 4; i++) {
    if(board[x] == board[y] && board[y] == board[z] && board[x] != 0) {
      //printf("Mühle erkannt bei %d-%d-%d von %d\n", x, y, z, board[x]);
      if(board[x] == 1) {
        result += value;
        millsClient++;
      }else if(board[x] == -1) {
        result -= value;
        millsOpponent++;
      }
    }
    x = (x+2) % 8;
    y = (y+2) % 8;
    z = (z+2) % 8;
  }
  // B
  x = 8;
  y = 9;
  z = 10;
  for(int i = 0; i < 4; i++) {
    if(board[x] == board[y] && board[y] == board[z] && board[x] != 0) {
      //printf("Mühle erkannt bei %d-%d-%d von %d\n", x, y, z, board[x]);
      if(board[x] == 1) {
        result += value;
        millsClient++;
      }else if(board[x] == -1) {
        result -= value;
        millsOpponent++;
      }
    }
    x = (x+2) % 8 + 8;
    y = (y+2) % 8 + 8;
    z = (z+2) % 8 + 8;
  }
  // C
  x = 16;
  y = 17;
  z = 18;
  for(int i = 0; i < 4; i++) {
    if(board[x] == board[y] && board[y] == board[z] && board[x] != 0) {
      //printf("Mühle erkannt bei %d-%d-%d von %d\n", x, y, z, board[x]);
      if(board[x] == 1) {
        result += value;
        millsClient++;
      }else if(board[x] == -1) {
        result -= value;
        millsOpponent++;
      }
    }
    x = (x+2) % 8 + 16;
    y = (y+2) % 8 + 16;
    z = (z+2) % 8 + 16;
  }
  // Mills in between Rings
  x = 1;
  y = 9;
  z = 17;
  for(int i = 0; i < 4; i++) {
    if(board[x] == board[y] && board[y] == board[z] && board[x] != 0) {
      //printf("Mühle erkannt bei %d-%d-%d von %d\n", x, y, z, board[x]);
      if(board[x] == 1) {
        result += value;
        millsClient++;
      }else if(board[x] == -1) {
        result -= value;
        millsOpponent++;
      }
    }
    x = x + 2;
    y = y + 2;
    z = z + 2;
  }
  //printf("Der Client hat %d und der Gegener %d Mühlen.\n", millsClient, millsOpponent);
  return result;
}


void makeMove(bool player, int* board, int move[2]) {
  int pieceColor = 0;
  if(player) {
    pieceColor = 1;
  } else {
    pieceColor = -1;
  }
  board[move[0]] = 0;
  board[move[1]] = pieceColor;
}

void undoMove(bool player, int* board, int move[2]) {
  int pieceColor = 0;
  if(player) {
    pieceColor = 1;
  } else {
    pieceColor = -1;
  }
  board[move[0]] = pieceColor;
  board[move[1]] = 0;
}


int generiereMoeglicheZuege(bool player, int* boardMiniMax, int b, int possibleMoves[][b]){
  static char movesNumbers[64][5] = {
      // 12 position with 2 possible moves
      "0007\0", "0001\0", "0201\0", "0203\0", "0403\0", "0405\0", "0605\0",
      "0607\0", "0815\0", "0809\0", "1009\0", "1011\0", "1211\0", "1213\0",
      "1413\0", "1415\0", "1623\0", "1617\0", "1817\0", "1819\0", "2019\0",
      "2021\0", "2221\0", "2223\0",
      // 8 positions with 3 possible moves
      "0100\0", "0102\0", "0109\0", "0302\0", "0304\0", "0311\0", "0504\0",
      "0506\0", "0513\0", "0706\0", "0700\0", "0715\0", "1716\0", "1718\0",
      "1709\0", "1918\0", "1920\0", "1911\0", "2120\0", "2122\0", "2113\0",
      "2322\0", "2316\0", "2315\0",
      // 4 positions with 4 possible moves
      "0908\0", "0910\0", "0901\0", "0917\0", "1110\0", "1112\0", "1103\0",
      "1119\0", "1312\0", "1314\0", "1305\0", "1321\0", "1514\0", "1508\0",
      "1507\0", "1523\0"};

  int playerNum = 0;
  if(player) playerNum = 1; else playerNum = -1;
  int countPossibleMoves = 0;

  bool board[SIZE_OF_BOARD];
  // fill array with false
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    board[i] = false;
  }

  //setze alle positionen im board auf true auf denen ein piece steht
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    if (boardMiniMax[i] != 0) { //Positionen auf denen ein Piece steht auf true setzen 
      board[i] = true;
    }
  }

  //gehe Board durch und schreibe alle möglichen spielzüge in possibleMoves
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    if (boardMiniMax[i] == playerNum) {
      for (int j = 0; j < 64; j++) { //gehe movesNumbers durch und schaue für jeden zug ob dieser möglich ist
        char start[3];
        strncpy(start, movesNumbers[j], 2);
        start[2] = '\0';
        if (atoi(start) == i) {
          char end[3];
          strncpy(end, movesNumbers[j] + 2, 3);
          if (!board[atoi(end)]) {

            possibleMoves[countPossibleMoves][0] = atoi(start);
            possibleMoves[countPossibleMoves][1] = atoi(end);
            countPossibleMoves++;
  
          }
        }
      }
    }
  }
  return countPossibleMoves;
}

bool keineZuegeMehr(bool player, int* boardMiniMax, int b, int possibleMoves[][b]){
  int countPossibleMoves = -1;
  countPossibleMoves = generiereMoeglicheZuege(player, boardMiniMax, b, possibleMoves);
  if(countPossibleMoves == -1){ //Sollte nie passieren
    printf("Fehler bei keineZuegeMehr().\n");
    exit(EXIT_FAILURE);
  }else if (countPossibleMoves == 0){
    return true;
  } else {
    return false;
  }

}


bool think(int fd[2], int n, int sharedMemoryID) {
  clock_t time;
  time = clock();
  printBoard(sharedMemoryID);
  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);

  // Variablen um Spielzug nacher in pipe zu schreiben
  int result = 0;
  char move[MAX_MOVE_LENGTH] = "";

  // CAPTURE != 0
  if (sharedMemoryPtr->currentMove.captureAmount != 0) {
    char boardPositions[24][MAX_MOVE_LENGTH] = {
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "B0", "B1", "B2", "B3",
        "B4", "B5", "B6", "B7", "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7"};
    printf("Du musst %d Stein(e) schlagen!\n",
           sharedMemoryPtr->currentMove.captureAmount);
    int opponentPieceAmount = 0;
    int opponentPieces[9];
    for (int i = 0; i < 9; i++) {
      opponentPieces[i] = -1;
    }
    for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
      if(sharedMemoryPtr->currentMove.pieces[i].playerID != sharedMemoryPtr->clientPlayerID &&
      !sharedMemoryPtr->currentMove.pieces[i].isAvailable &&
      !sharedMemoryPtr->currentMove.pieces[i].isCaptured) {
        opponentPieces[opponentPieceAmount] = sharedMemoryPtr->currentMove.pieces[i].position;
        opponentPieceAmount++;
      }
    }
    int randomCapture = generateRandomNumber(0, opponentPieceAmount - 1);
    if(opponentPieces[randomCapture] == -1) {
      printf("Etwas ist schiefgelaufen bei der randomisierten Auswahl eines Steines für Capture!\n");
      exit(EXIT_FAILURE);
    }else {
      strcpy(move, boardPositions[opponentPieces[randomCapture]]);
    }

    /*for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
      if (sharedMemoryPtr->currentMove.pieces[i].playerID !=
              sharedMemoryPtr->clientPlayerID &&
          !sharedMemoryPtr->currentMove.pieces[i].isAvailable &&
          !sharedMemoryPtr->currentMove.pieces[i].isCaptured) {

        strcpy(move,
               boardPositions[sharedMemoryPtr->currentMove.pieces[i].position]);
        break;
      }
    }*/
  } else if (countMoves <= 8) {
    printf("Wir sind in der Setzphase!\n");
    placePiece(move, sharedMemoryID);
  } else if (countMoves > 8) {
    printf("Wir sind in der Zugphase!\n");
    //movePiece(move);
    int board[SIZE_OF_BOARD];
    // fill array with 0
    for (int i = 0; i < SIZE_OF_BOARD; i++) {
      board[i] = 0;
    }
    for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
      if (sharedMemoryPtr->currentMove.pieces[i].position != -1) {
        if(sharedMemoryPtr->currentMove.pieces[i].playerID == sharedMemoryPtr->clientPlayerID) {
          board[sharedMemoryPtr->currentMove.pieces[i].position] = 1;
        }else {
          board[sharedMemoryPtr->currentMove.pieces[i].position] = -1;
        }
      }
    }
    int bestMove[2];
    int returnMiniMax;
    returnMiniMax = miniMax(true, 3, 3, board, bestMove);
    printf("Bewertung vom MiniMax Algorithmus: %d\n", returnMiniMax);
    strcpy(move, boardPositions[bestMove[0]]);
    strcpy(move+2, ":");
    strcpy(move+3, boardPositions[bestMove[1]]);
    countMoves++;
  } else {
    printf("Das sollte nicht passieren\n");
    assert(false);
  }

  detachSharedMemory(sharedMemoryPtr);

  // Spielzug in pipe schreiben
  result = write(fd[1], move, n);
  if (result != n) {
    printf("Fehler bei write().\n");
    return false;
  }
  time = clock() - time;
  double time_taken = ((double)time)/CLOCKS_PER_SEC; // in seconds
  if(time_taken > max_time) {
    max_time = time_taken;
  }
  printf("zeit benötigt für zug: %f", max_time);
  return true;
}

void placePiece(char *move, int sharedMemoryID) {

  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  bool board[SIZE_OF_BOARD];
  int possiblePositions[SIZE_OF_BOARD];
  int possiblePositionCount = 0;
  int currentBoardPosition = 0;
  int randomPlacePosition = 0;

  // fill array with false
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    board[i] = false;
  }
  for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
    if (sharedMemoryPtr->currentMove.pieces[i].position != -1) {
      currentBoardPosition = sharedMemoryPtr->currentMove.pieces[i].position;
      board[currentBoardPosition] = true;
    }
  }

  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    if (!board[i]) {
      possiblePositions[possiblePositionCount] = i;
      possiblePositionCount++;
    }
  }
  randomPlacePosition =
      possiblePositions[generateRandomNumber(0, possiblePositionCount - 1)];
  strcpy(move, boardPositions[randomPlacePosition]);
  countMoves++;
  detachSharedMemory(sharedMemoryPtr);
}

void movePiece(char *move) {
  static char movesNumbers[64][5] = {
      // 12 position with 2 possible moves
      "0007\0", "0001\0", "0201\0", "0203\0", "0403\0", "0405\0", "0605\0",
      "0607\0", "0815\0", "0809\0", "1009\0", "1011\0", "1211\0", "1213\0",
      "1413\0", "1415\0", "1623\0", "1617\0", "1817\0", "1819\0", "2019\0",
      "2021\0", "2221\0", "2223\0",
      // 8 positions with 3 possible moves
      "0100\0", "0102\0", "0109\0", "0302\0", "0304\0", "0311\0", "0504\0",
      "0506\0", "0513\0", "0706\0", "0700\0", "0715\0", "1716\0", "1718\0",
      "1709\0", "1918\0", "1920\0", "1911\0", "2120\0", "2122\0", "2113\0",
      "2322\0", "2316\0", "2315\0",
      // 4 positions with 4 possible moves
      "0908\0", "0910\0", "0901\0", "0917\0", "1110\0", "1112\0", "1103\0",
      "1119\0", "1312\0", "1314\0", "1305\0", "1321\0", "1514\0", "1508\0",
      "1507\0", "1523\0"};
  static char movesNames[64][6] = {
      // 12 position with 2 possible moves
      "A0:A7\0", "A0:A1\0", "A2:A1\0", "A2:A3\0", "A4:A3\0", "A4:A5\0",
      "A6:A5\0", "A6:A7\0", "B0:B7\0", "B0:B1\0", "B2:B1\0", "B2:B3\0",
      "B4:B3\0", "B4:B5\0", "B6:B5\0", "B6:B7\0", "C0:C7\0", "C0:C1\0",
      "C2:C1\0", "C2:C3\0", "C4:C3\0", "C4:C5\0", "C6:C5\0", "C6:C7\0",
      // 8 positions with 3 possible moves
      "A1:A0\0", "A1:A2\0", "A1:B1\0", "A3:A2\0", "A3:A4\0", "A3:B3\0",
      "A5:A4\0", "A5:A6\0", "A5:B5\0", "A7:A6\0", "A7:A0\0", "A7:B7\0",
      "C1:C0\0", "C1:C2\0", "C1:B1\0", "C3:C2\0", "C3:C4\0", "C3:B3\0",
      "C5:C4\0", "C5:C6\0", "C5:B5\0", "C7:C6\0", "C7:C0\0", "C7:B7\0",
      // 4 positions with 4 possible moves
      "B1:B0\0", "B1:B2\0", "B1:A1\0", "B1:C1\0", "B3:B2\0", "B3:B4\0",
      "B3:A3\0", "B3:C3\0", "B5:B4\0", "B5:B6\0", "B5:A5\0", "B5:C5\0",
      "B7:B6\0", "B7:B0\0", "B7:A7\0", "B7:C7\0"};

  char possibleMoves[64][6];
  int countPossibleMoves = 0;

  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  bool board[SIZE_OF_BOARD];
  // fill array with false
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    board[i] = false;
  }
  for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
    if (sharedMemoryPtr->currentMove.pieces[i].position != -1) {
      board[sharedMemoryPtr->currentMove.pieces[i].position] = true;
    }
  }
  for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
    if (sharedMemoryPtr->currentMove.pieces[i].playerID ==
            sharedMemoryPtr->clientPlayerID &&
        !sharedMemoryPtr->currentMove.pieces[i].isAvailable &&
        !sharedMemoryPtr->currentMove.pieces[i].isCaptured) {
      for (int j = 0; j < 64; j++) {
        char start[3];
        strncpy(start, movesNumbers[j], 2);
        start[2] = '\0';
        if (atoi(start) == sharedMemoryPtr->currentMove.pieces[i].position) {
          char end[3];
          strncpy(end, movesNumbers[j] + 2, 3);
          if (!board[atoi(end)]) {

            strcpy(possibleMoves[countPossibleMoves], movesNames[j]);
            countPossibleMoves++;
            // countMoves++;
            // printf("MOVE DEN WIR SENDEN: %s\n", move);
            // detachSharedMemory(sharedMemoryPtr);
            // return;
          }
        }
      }
    }
  }
  printf("Liste an möglichen Zügen:\n");
  for (int i = 0; i < countPossibleMoves; i++) {
    printf("%s\n", possibleMoves[i]);
  }

  int moveNumber = 0;
  moveNumber = generateRandomNumber(0, countPossibleMoves - 1);
  printf("%d", moveNumber);
  strncpy(move, possibleMoves[moveNumber], 5);
  printf("MOVE DEN WIR SENDEN: %s\n", move);
  detachSharedMemory(sharedMemoryPtr);
}

void printBoard(int sharedMemoryID) {
  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  // parse board
  char playerSymbols[] = {'*', '#'}; // 2 players !
  char board[SIZE_OF_BOARD];
  // fill array with '+'
  for (int i = 0; i < SIZE_OF_BOARD; i++) {
    board[i] = '+';
  }
  // set positions where player has a piece
  for (int i = 0; i < sharedMemoryPtr->currentMove.pieceAmount; i++) {
    if (sharedMemoryPtr->currentMove.pieces[i].position != -1) {
      board[sharedMemoryPtr->currentMove.pieces[i].position] =
          playerSymbols[sharedMemoryPtr->currentMove.pieces[i].playerID];
    }
  }
  // print game
  printf(
      "%c%c%c%c%s%c%c%c%s%c%c%c\n"
      " |           |           |\n"
      " |  %c%c%c%c%s%c%c%c%s%c%c%c%c  |\n"
      " |   |       |       |   |\n"
      " |   |  %c%c%c%c%s%c%c%c%s%c%c%c%c  |   |\n"
      " |   |   |       |   |   |\n"
      "%c%c%c%c%s%c%c%c%s%c%c%c%c     %c%c%c%c%s%c%c%c%s%c%c%c\n"
      " |   |   |       |   |   |\n"
      " |   |  %c%c%c%c%s%c%c%c%s%c%c%c%c  |   |\n"
      " |   |       |       |   |\n"
      " |  %c%c%c%c%s%c%c%c%s%c%c%c%c  |\n"
      " |           |           |\n"
      "%c%c%c%c%s%c%c%c%s%c%c%c\n",
      // außen oben
      (board[0] != '+') ? '\0' : ' ', (board[0] != '+') ? '(' : '\0', board[0],
      (board[0] != '+') ? ')' : '\0', minus(board[0], board[1], 1),
      (board[1] != '+') ? '(' : '\0', board[1], (board[1] != '+') ? ')' : '\0',
      minus(board[1], board[2], 1), (board[2] != '+') ? '(' : '\0', board[2],
      (board[2] != '+') ? ')' : '\0',
      // mitte oben
      (board[8] != '+') ? '\0'
                        : ' ', // ein Leerzeichen mehr, wenn keine Klammer
      (board[8] != '+') ? '(' : '\0', board[8], (board[8] != '+') ? ')' : '\0',
      minus(board[8], board[9], 2), (board[9] != '+') ? '(' : '\0', board[9],
      (board[9] != '+') ? ')' : '\0', minus(board[9], board[10], 2),
      (board[10] != '+') ? '(' : '\0', board[10],
      (board[10] != '+') ? ')' : '\0', (board[10] != '+') ? '\0' : ' ',
      // innen oben
      (board[16] != '+') ? '\0' : ' ', (board[16] != '+') ? '(' : '\0',
      board[16], (board[16] != '+') ? ')' : '\0',
      minus(board[16], board[17], 3), (board[17] != '+') ? '(' : '\0',
      board[17], (board[17] != '+') ? ')' : '\0',
      minus(board[17], board[18], 3), (board[18] != '+') ? '(' : '\0',
      board[18], (board[18] != '+') ? ')' : '\0',
      (board[18] != '+') ? '\0' : ' ',
      // Mitte / wagerechte Verbindungsteile
      (board[7] != '+') ? '\0' : ' ', (board[7] != '+') ? '(' : '\0', board[7],
      (board[7] != '+') ? ')' : '\0', minus(board[7], board[15], 3),
      (board[15] != '+') ? '(' : '\0', board[15],
      (board[15] != '+') ? ')' : '\0', minus(board[15], board[23], 3),
      (board[23] != '+') ? '(' : '\0', board[23],
      (board[23] != '+') ? ')' : '\0', (board[23] != '+') ? '\0' : ' ',
      (board[19] != '+') ? '\0' : ' ', (board[19] != '+') ? '(' : '\0',
      board[19], (board[19] != '+') ? ')' : '\0',
      minus(board[19], board[11], 3), (board[11] != '+') ? '(' : '\0',
      board[11], (board[11] != '+') ? ')' : '\0', minus(board[11], board[3], 3),
      (board[3] != '+') ? '(' : '\0', board[3], (board[3] != '+') ? ')' : '\0',
      // innen unten
      (board[22] != '+') ? '\0' : ' ', (board[22] != '+') ? '(' : '\0',
      board[22], (board[22] != '+') ? ')' : '\0',
      minus(board[22], board[21], 3), (board[21] != '+') ? '(' : '\0',
      board[21], (board[21] != '+') ? ')' : '\0',
      minus(board[21], board[20], 3), (board[20] != '+') ? '(' : '\0',
      board[20], (board[20] != '+') ? ')' : '\0',
      (board[20] != '+') ? '\0' : ' ',
      // mitte unten
      (board[14] != '+') ? '\0' : ' ', (board[14] != '+') ? '(' : '\0',
      board[14], (board[14] != '+') ? ')' : '\0',
      minus(board[14], board[13], 2), (board[13] != '+') ? '(' : '\0',
      board[13], (board[13] != '+') ? ')' : '\0',
      minus(board[13], board[12], 2), (board[12] != '+') ? '(' : '\0',
      board[12], (board[12] != '+') ? ')' : '\0',
      (board[12] != '+') ? '\0' : ' ',
      // außen unten
      (board[6] != '+') ? '\0' : ' ', (board[6] != '+') ? '(' : '\0', board[6],
      (board[6] != '+') ? ')' : '\0', minus(board[6], board[5], 1),
      (board[5] != '+') ? '(' : '\0', board[5], (board[5] != '+') ? ')' : '\0',
      minus(board[5], board[4], 1), (board[4] != '+') ? '(' : '\0', board[4],
      (board[4] != '+') ? ')' : '\0');
  detachSharedMemory(sharedMemoryPtr);
}

char *minus(char first, char second, int mode) {
  if (first == '+' && second == '+') {
    switch (mode) {
    case 1:
      return "-----------"; // elf
    case 2:
      return "-------"; // sieben
    case 3:
      return "---"; // drei
    default:
      printf("Ein falscher Modus wurde übergeben.\n");
      break;
    }
  } else if (first == '+' || second == '+') {
    switch (mode) {
    case 1:
      return "----------"; // zehn
    case 2:
      return "------"; // sechs
    case 3:
      return "--"; // zwei
    default:
      printf("Ein falscher Modus wurde übergeben.\n");
      break;
    }
  } else {
    switch (mode) {
    case 1:
      return "---------"; // neun
    case 2:
      return "-----"; // fünf
    case 3:
      return "-"; // eins
    default:
      printf("Ein falscher Modus wurde übergeben.\n");
      break;
    }
  }
  return "-";
}

int initSocket(GameServerConfig *config) {
  struct sockaddr_in address;
  int sock = socket(PF_INET, SOCK_STREAM, 0);
  address.sin_family = AF_INET;
  address.sin_port = htons(config->portNumber);

  // ip adresse des servers herausfinden
  struct hostent *host;
  struct in_addr **ip_ptr; // ip;

  host = gethostbyname(config->hostname);
  if (host == NULL) {
    printf("Fehler bei gethostbyname!\n");
    exit(EXIT_FAILURE);
  }
  char *serverIp = 0;
  ip_ptr = (struct in_addr **)host->h_addr_list;
  if (*ip_ptr != NULL)
    serverIp = inet_ntoa(**ip_ptr++);

  // mit Server verbinden
  sock = socket(AF_INET, SOCK_STREAM, 0);
  inet_aton(serverIp, &address.sin_addr);
  if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
    printf("Verbindung mit %s hergestellt.\n", inet_ntoa(address.sin_addr));
  } else {
    printf("Fehler beim Verbindungsaufbau mit %s.\n",
           inet_ntoa(address.sin_addr));
    exit(EXIT_FAILURE);
  }
  return sock;
}

int createSharedMemory(size_t size) {
  int shmid = shmget(IPC_PRIVATE, size, 0644 | IPC_CREAT);
  if (shmid == -1) {
    perror("Clientfehler:");
    exit(EXIT_FAILURE);
  }
  return shmid;
}

void createPipe(int fd[2]) {
  fd[0] = fd[1] = 0;
  if (pipe(fd) == -1) {
    printf("Fehler beim Einrichten der Pipe.\n");
    exit(EXIT_FAILURE);
  }
}

int createFork() {
  int pid = fork();
  if (pid < 0) {
    printf("Fehler bei fork().\n");
    exit(EXIT_FAILURE);
  }
  return pid;
}

void writePIDSharedMemory(int sharedMemoryID, int childPID) {
  SharedMemory *sharedMemoryPtr = attachSharedMemory(sharedMemoryID);
  sharedMemoryPtr->connecterPID = childPID;
  sharedMemoryPtr->thinkerPID = getpid();
  detachSharedMemory(sharedMemoryPtr);
}

int generateRandomNumber(
    int lower, int upper) { // generates random number in range [lower, upper]
  int num = 0;
  num = (rand() % (upper - lower + 1)) + lower;
  return num;
}
