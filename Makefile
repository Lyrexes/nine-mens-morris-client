SRC = sysprak-client.c performConnection.c config.c prolog.c
CC = gcc
CFLAGS =  -Wall -Werror -ggdb3

sysprak-client: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) 
clean:
	rm -rf sysprak-client
#play: sysprak-client ./sysprak-client -g $(GAME_ID) -p $(PLAYER)
