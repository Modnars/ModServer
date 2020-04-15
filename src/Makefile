CC 	=g++
CFLAGS	=-Wall
SERVER  =server
_SERVER =ModServer
CLIENT	=old_client
_CLIENT =ModClient
DEBUGER =lldb
HEADER	=include

all :
	$(CC) $(CFLAGS) -I$(HEADER) $(SERVER).cpp -o $(_SERVER).out
	$(CC) $(CFLAGS) -I$(HEADER) $(CLIENT).cpp -o $(_CLIENT).out

d :
	$(CC) $(CFLAGS) $(SERVER).cpp -o $(_SERVER).out -g
	$(DEBUGER) $(_SERVER).out

s :
	./$(_SERVER).out 127.0.0.1 7777

c :
	./$(_CLIENT).out 127.0.0.1 7777

clean :
	-rm -rf *.out *.dSYM
