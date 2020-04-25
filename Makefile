all: sharedFunctions.o
	gcc -o client/WTF client/client.c sharedFunctions.o -lssl -lcrypto
	gcc -o server/WTFserver server/server.c sharedFunctions.o -pthread

sharedFunctions.o: sharedFunctions.c
	gcc -c sharedFunctions.c

clean: sharedFunctions.o client/WTF server/WTFserver
	rm sharedFunctions.o
	rm client/WTF
	rm server/WTFserver