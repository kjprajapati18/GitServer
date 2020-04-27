#ifndef AVL_H
#define AVL_H

//tree nodes hold a string and an int- word and freq
typedef struct  _avlNode{
    char* ver;
    int verNum;
    char* path;
    char* code;
    struct _avlNode* left;
    struct _avlNode* right;
    int height;
}avlNode;

//find max of 2 ints
int max(int, int);
//get Height of node
int height(avlNode*);
//Make node given token&bitString
avlNode* makeNode(char*, char*, char*);
//Right Rotate for AVL
avlNode* RR(avlNode*);
//Left Rotate for AVL
avlNode* LR(avlNode*);
//Get balance factor of Node
int balanceFactor(avlNode*);
//insert a node to AVL and rotate if needed
avlNode* insert(avlNode*, char*, char*, char*);
//Free AVL tree
void freeAvl(avlNode*);
//Print AVL tree, used for testing only
//void print2DTree(avlNode* root, int space);
//Find AVL Node given the token
int findAVLNode(avlNode**, avlNode*, char*);
avlNode* fillAvl(char**);
void advanceToken(char**, char);
void printAVLList(avlNode*);

#endif