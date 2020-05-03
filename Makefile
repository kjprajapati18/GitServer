all: sharedFunctions.o avl.o serverOperations.o linkedList.o
	gcc -o client/WTF client/client.c sharedFunctions.o avl.o -lssl -lcrypto
	gcc -o server/WTFserver server/server.c sharedFunctions.o avl.o serverOperations.o linkedList.o -pthread -lssl -lcrypto

sharedFunctions.o: sharedFunctions.c
	gcc -c sharedFunctions.c -lssl -lcrypto

avl.o: avl.c
	gcc -c avl.c

serverOperations.o: serverOperations.c
	gcc -c serverOperations.c -pthread

linkedList.o: linkedList.c
	gcc -c linkedList.c -pthread

clean: avl.o sharedFunctions.o serverOperations.o linkedList.o client/WTF server/WTFserver
	rm sharedFunctions.o
	rm avl.o
	rm serverOperations.o
	rm linkedList.o
	rm client/WTF
	rm server/WTFserver