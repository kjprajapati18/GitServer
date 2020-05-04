#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "avl.h"
#include "sharedFunctions.h"
#define COUNT 10

//helper method to find max of 2 numbers. returns 2nd number if equal
int max(int a, int b){
    return (a>b)? a: b;
}

//returns height of tree from specific node
int height(avlNode* head){
    return (head==NULL)? 0 : head->height;
}

//mallocs new node and sets initial values
avlNode* makeNode(char* ver, char* path, char* code){
    avlNode* newNode = (avlNode*) malloc(sizeof(avlNode));
    newNode->ver = ver;
    newNode->verNum = atoi(ver);
    newNode->path = path;
    newNode->code = code;
    newNode->left = NULL;
    newNode->right = NULL;
    newNode->height = 1;
    return newNode;
}

//right rotation helper method to help with rebalancing
avlNode* RR(avlNode* head){
    avlNode* newHead = head->left;
    avlNode* changes = newHead->right;

    head->left = changes;
    newHead->right = head;

    head->height = max(height(head->left), height(head->right))+1;                        
    newHead->height = max(height(newHead->left), height(newHead->right))+1;

    return newHead;
}

//left rotation helper method
avlNode* LR(avlNode* head){
    avlNode* newHead = head->right;
    avlNode* changes = newHead->left;

    head->right = changes;
    newHead->left = head;

    head->height = max(height(head->left), height(head->right))+1;
    newHead->height = max(height(newHead->left), height(newHead->right))+1;

    return newHead;
}

//Gets the balance factor of a Node
int balanceFactor(avlNode* head){
    return (head == NULL)? 0: height(head->left) - height(head->right);
}

//Inserts a token if it is not already in the tree, otherwise it just increases the frequency. Double checks balances and rotates if necessary
avlNode* insert(avlNode* node, char* ver, char* path, char* code){
    
    if(node == NULL){
        return makeNode(ver, path, code);
    } 

    if(strcmp(path, node->path) < 0)
        node->left = insert(node->left, ver, path, code);
    else if(strcmp(path, node->path) > 0)
        node->right = insert(node->right, ver, path, code);
    else{
        printf("Error: This node already exists in tree\n");
        return node;
    }

    node->height = 1 + max(height(node->left), height(node->right));
    int balance = balanceFactor(node);
    
    //left right or left left
    if(balance > 1){
        //left left
        if(balanceFactor(node->left) >= 0){
            return RR(node);
            
        }
        //left right
        else{
            node->left = LR(node->left);
            return RR(node);
        }
    }
    
    //right right or right left
    else if(balance < -1){
        //right right
        if (balanceFactor(node->right) <= 0){
            return LR(node);
        }
        //right left
        else {
            node->right = RR(node->right);
            return LR(node);
        }
    }
    
    return node;
}

//Free the AVL Nodes (note that no char* were malloc'd, they pointed to already malloc'd strings)
void freeAvl(avlNode* head){
    avlNode* l = head->left;
    avlNode* r = head->right;
    free(head);
    if(l != NULL) freeAvl(l);
    if(r != NULL) freeAvl(r);
}


//Finds the AVLNode that contains token, and returns it in selectedNode. Returns 0 if successful and negative number otherwise
int findAVLNode(avlNode** selectedNode, avlNode* head, char* token){
    int status = 0;
    if(token[0] == '\0') return -2; //There are no empty strings
    if(head == NULL){               //Didn't find it
        return -1;
    }

    if(strcmp(token, head->path) < 0)
        status = findAVLNode(selectedNode, head->left, token);
    else if(strcmp(token, head->path) > 0)
        status = findAVLNode(selectedNode, head->right, token);
    else{
        *selectedNode = head;
    }
    return status;
}

//Ptr should initially be after the first new line (start at first entry)
avlNode* fillAvl(char** manifestPtr){
    
    avlNode* head = NULL;
    char *fileVer, *filePath, *fileHash; //Store the start of each line
    
    while(*(*manifestPtr) != '\0'){
        //Points to beginning of File version (so we are not duplicating any data)
        fileVer = *manifestPtr;
        //finds the next space, since that is the end of the file version.
        //Sets that space to be \0 so that file version string can be accessed by FileVer pointers;
        //Advance the ptr 1 extra time to get to the start of the next token
        //Convert file version to int
        advanceToken(manifestPtr, ' ');

        //Perform a similar logic to extract path string and Hash code string
        filePath = *manifestPtr;
        advanceToken(manifestPtr, ' ');

        fileHash = *manifestPtr;
        advanceToken(manifestPtr, '\n');

        //Add node to AVL
        head = insert(head, fileVer, filePath, fileHash);
    }
    
    return head;
}

//Assumption is that \0 is never the delimiter (we only call with ' ' or '\n')
//Goes through a string (usually the data of a file) sets the first occurrence of the delimiter to a \0 (essentially splitting the string)
//The function then sets the passed in ptr to the character after this null token
void advanceToken(char** ptr, char delimiter){
    if(*(*ptr) == '\0') return;
    while(*(*ptr) != delimiter && *(*ptr) != '\0') (*ptr)++;
    if(*(*ptr) != '\0'){
        *(*ptr) = '\0';
        (*ptr)++;
    }
}

//prints AVL in pre-order
void printAVLList(avlNode* head){
    if(head == NULL) return;
    printf("File Ver: %s/%d\n", head->ver, head->verNum);
    printf("File Path: %s\n", head->path);
    printf("File Hash: %s\n\n", head->code);
    printAVLList(head->left);
    printAVLList(head->right);

}

//applies changes in commit file avl to manifest file avl so that writeTree() will write the correct strings to the file
avlNode* commitChanges(avlNode* commitHead, avlNode* manhead){
    //in order traversal of the commit file AVL
    if(commitHead == NULL) return manhead;
    manhead = commitChanges(commitHead->left, manhead);
    //switch case on the nature of the commit
    switch((commitHead->ver)[strlen(commitHead->ver) - 1]){
        case 'D':{
            //change version number to -1 so we know to skip it in writeTree(), essentially deleting it from the manifest
            avlNode* found;
            findAVLNode(&found, manhead, commitHead->path);
            found->verNum = -1;
            break;}
        case 'M':{
            //increment version number in the manifest and change the hash code
            avlNode* found;
            findAVLNode(&found, manhead, commitHead->path);
            (found->verNum)++;
            found->code = commitHead->code;
            break;}
        case 'A':{
            //change version number to -2 so we know it was recently added in writeTree(), allowing us to write a version number of 0 in the manifest separate from branches untouched in the manifest avl with that version number
            char* zero = (char*) malloc(3);
            sprintf(zero, "-2");
            manhead = insert(manhead, zero, commitHead->path, commitHead->code);
            break;}
    }
    manhead = commitChanges(commitHead->right, manhead);
    return manhead;

}
//writes the avl information to a file descriptor
void writeTree(avlNode* head, int fd){
    //in order tree traversal
    if(head == NULL) return;
    writeTree(head->left, fd);
    //as long as the version number is not -1 (i.e. we deleted it) we write the node to a the file
    if(head->verNum != -1){
        char* verstring = (char*) malloc(12);
        if(head->verNum == -2) sprintf(verstring, "%d", 0); //write 0 as the file's version number for files we specified as -2 in commitChanges()
        else sprintf(verstring, "%d", head->verNum); //take the file version number string as what's given if it's not -2
        //write the information in the manifest's format
        writeString(fd, verstring);
        writeString(fd, " ");
        writeString(fd, head->path);
        writeString(fd, " ");
        writeString(fd, head->code);
        writeString(fd, "\n");

        free(verstring);
        if(!strcmp(head->ver, "-2")) free(head->ver);
    }
    writeTree(head->right, fd);
}