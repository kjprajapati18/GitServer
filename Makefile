all: sharedFunctions.o avl.o
	gcc -o client/WTF client/client.c sharedFunctions.o avl.o -lssl -lcrypto
	gcc -o server/WTFserver server/server.c sharedFunctions.o avl.o -pthread -lssl -lcrypto

sharedFunctions.o: sharedFunctions.c
	gcc -c sharedFunctions.c -lssl -lcrypto

avl.o: avl.c
	gcc -c avl.c

clean: avl.o sharedFunctions.o client/WTF server/WTFserver
	rm sharedFunctions.o
	rm avl.o
	rm client/WTF
	rm server/WTFserver