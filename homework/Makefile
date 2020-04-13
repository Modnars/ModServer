CC 	=g++
CFLAGS	=-Wall
SERVER  =chatServer
_SERVER =ModServer
CLIENT	=client
_CLIENT =ModClient
DEBUGER =lldb

all :
	$(CC) $(CFLAGS) $(SERVER).cpp -o $(_SERVER).out
	$(CC) $(CFLAGS) $(CLIENT).cpp -o $(_CLIENT).out

d :
	$(CC) $(CFLAGS) $(SERVER).cpp -o $(_SERVER).out -g
	$(DEBUGER) $(_SERVER).out

s :
	./$(_SERVER).out 127.0.0.1 7777

c :
	./$(_CLIENT).out 127.0.0.1 7777

clean :
	-rm -rf *.out *.dSYM
