#pragma once
#include <stdbool.h>

typedef struct List
{
    struct Node* head;
    struct Node* tail;
} List;

typedef struct Node
{
    int fd;
    struct Node* next;
    struct Node* prev;
} Node;

Node* NodeCreate(int fd);

List* ListCreate();

bool ListAppend(List* list, int fd);

void ListRemove(List* list, Node* node);

void NodeDestroy(Node* node);

void ListDestroy(List* head);
