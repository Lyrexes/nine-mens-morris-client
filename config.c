#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define STDCONF "client.conf"

GameServerConfig parseArguments(int argc, char* argv[]) {
  GameServerConfig config;
  int ret = 0;
  char *gameId = NULL;
  int playerNumber = -1;
  char *configfile;
  bool configreceived = false;
  bool playerNumberReceived = false;
  while ((ret = getopt(argc, argv, "g:p:c:")) != -1) {
    switch (ret) {
    case 'g':
      gameId = optarg;
      break;
    case 'p':
      playerNumber = atoi(optarg) - 1;
      playerNumberReceived = true;
      break;
    case 'c':
      configfile = optarg;
      configreceived = true;
      break;
    }
  }

  // Ausgabe ob Parameter erfolgreich eingelesen wurden & gültig sind
  // Game-ID und Config
  if (gameId == NULL || strlen(gameId) != 13) {
    printf("Fehler beim Aufruf des Programms! Bitte 13-stellige Game-Id angeben.\n");
    exit(EXIT_FAILURE);
  } else if (configreceived) {
    printf("Game-ID (%s) wurde übernommen.\nName der Konfigurationsdatei: %s\n", gameId, configfile);
  } else {
    configfile = STDCONF;
    printf("Game-ID (%s) wurde übernommen.\nStandardkonfigurationsdatei (%s) wird verwendet.\n", gameId, STDCONF);
  }
  // player Number
  if(playerNumberReceived && playerNumber < 0){
    printf("Ungültige Eingabe! Mitspielernummern beginnen bei 1.\n");
    exit(EXIT_FAILURE);
  }

  // Leerzeichen in der KOnfigurationsdatei entfernen
  removeBlankSpaces(configfile);

  // HOSTNAME
  char *_hostname = readValueAsString(configfile, "hostname");
  if(_hostname != NULL) {
    strcpy(config.hostname, _hostname);
    free(_hostname);
  }else{
    printf("Fehler beim Einlesen von Hostname aus der Konfigurationsdatei!\n");
    exit(EXIT_FAILURE);
  }

  // GAMEKINDNAME
  char *_gamekind = readValueAsString(configfile, "gamekind");
  if(_gamekind != NULL) {
    strcpy(config.gameKindName, _gamekind);
    free(_gamekind);
  }else{
    printf("Fehler beim Einlesen von Gamekind aus der Konfigurationsdatei!\n");
    exit(EXIT_FAILURE);
  }

  // PORTNUMBER
  char *_portnumber = readValueAsString(configfile, "portnumber");
  if(_portnumber != NULL) {
      unsigned int __portnumber;
      __portnumber = atoi(_portnumber);
      config.portNumber = __portnumber;
      free(_portnumber);
  }else{
    printf("Fehler beim Einlesen von Portnummer aus der Konfigurationsdatei!\n");
    exit(EXIT_FAILURE);
  }

  config.playerNumber = playerNumber;
  config.gameID = gameId;
  return config;
}

bool removeBlankSpaces(char* filename) {
    FILE *read;
    FILE *write;
    char remove = ' ';

    read = fopen(filename, "r");
    write = fopen(filename, "r+");

    if(read == NULL || write == NULL) {
        printf("Fehler beim Öffnen der Datei.\n");
        fclose(read);
        fclose(write);
        return false;
    }

    char currentChar;
    int count = 0;

    while(1){
        currentChar = fgetc(read);
        if(feof(read)) break;
        if(ferror(read)) {
          printf("Fehler beim Lesen der Datei.\n");
          fclose(read);
          fclose(write);
        }

        if(currentChar != remove){
          fputc(currentChar, write);
          count++;
        }
    }

    if(truncate(filename, count) == -1) {
        printf("Fehler beim kürzen der Datei.\n");
        fclose(read);
        fclose(write);
        return false;
    }
    fclose(read);
    fclose(write);
    return true;
}

char *readValueAsString(char *configfile, char const *desired_name) {
    FILE *fp = fopen(configfile, "r");
    if(fp == NULL){
      printf("Fehler beim Öffnen der Datei %s!\n", configfile);
      exit(EXIT_FAILURE);
    }
    char name[128];
    char val[128];
    
    while(fscanf(fp, "%127[^=]=%127[^\n]%*c", name ,val) == 2) {
        if(strcasecmp(name, desired_name) == 0) {
          fclose(fp);
          return strdup(val);
        }
    }
    fclose(fp);
    return NULL;
}
