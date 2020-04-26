all: sharedFunctions.o avl.o
	gcc -o client/WTF client/client.c sharedFunctions.o avl.o -lssl -lcrypto
	gcc -o server/WTFserver server/server.c sharedFunctions.o -pthread

sharedFunctions.o: sharedFunctions.c
	gcc -c sharedFunctions.c

avl.o: avl.c
	gcc -c avl.c

clean: avl.o sharedFunctions.o client/WTF server/WTFserver
	rm sharedFunctions.o
	rm avl.o
	rm client/WTF
	rm server/WTFserver