CC 	=gcc
CFLAGS	=-Wall
SERVER  =server
_SERVER =ModServer
CLIENT	=client
_CLIENT =ModClient
DEBUGER =lldb

all :
	$(CC) $(CFLAGS) $(SERVER).c -o $(_SERVER).out
	$(CC) $(CFLAGS) $(CLIENT).c -o $(_CLIENT).out

d :
	$(CC) $(CFLAGS) $(SERVER).c -o $(_SERVER).out -g
	$(DEBUGER) $(_SERVER).out

s :
	./$(_SERVER).out 127.0.0.1 7777

c :
	./$(_CLIENT).out 127.0.0.1 7777

clean :
	-rm -rf *.out *.dSYM
