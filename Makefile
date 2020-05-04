all: client.c server.c sharedFunctions.o avl.o serverOperations.o linkedList.o clientOperations.o
	gcc -o client/WTF client.c sharedFunctions.o avl.o clientOperations.o -lssl -lcrypto
	gcc -o server/WTFserver server.c sharedFunctions.o avl.o serverOperations.o linkedList.o -pthread -lssl -lcrypto

sharedFunctions.o: sharedFunctions.c
	gcc -c sharedFunctions.c -lssl -lcrypto

avl.o: avl.c
	gcc -c avl.c

serverOperations.o: serverOperations.c
	gcc -c serverOperations.c -pthread

clientOperations.o: clientOperations.c
	gcc -c clientOperations.c

linkedList.o: linkedList.c
	gcc -c linkedList.c -pthread

clean: avl.o sharedFunctions.o serverOperations.o clientOperations.o linkedList.o client/WTF server/WTFserver
	rm sharedFunctions.o
	rm avl.o
	rm serverOperations.o
	rm linkedList.o
	rm clientOperations.o
	rm client/WTF
	rm server/WTFserver