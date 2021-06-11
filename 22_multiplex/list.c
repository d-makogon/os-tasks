#include <stdlib.h>

#include "list.h"

Node* NodeCreate(int fd)
{
    Node* newNode = (Node*)malloc(sizeof(*newNode));
    if (!newNode) return NULL;

    newNode->fd = fd;
    newNode->prev = NULL;
    newNode->next = NULL;
    return newNode;
}

List* ListCreate()
{
    List* list = (List*)malloc(sizeof(*list));
    if (!list)
    {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    return list;
}

bool ListAppend(List* list, int fd)
{
    if (!list) return false;

    Node* newNode = NodeCreate(fd);

    if (!newNode) return false;

    Node* prevTail = list->tail;
    if (prevTail)
    {
        newNode->prev = prevTail;
        prevTail->next = newNode;
    }
    if (!list->head)
    {
        list->head = newNode;
    }

    list->tail = newNode;
    return true;
}

void ListRemove(List* list, Node* node)
{
    if (!list || !node)
    {
        return;
    }
    if (list->head == node)
    {
        Node* newHead = node->next;
        if (list->tail == node)
        {
            list->tail = NULL;
        }
        list->head = newHead;
    }
    else if (list->tail == node)
    {
        list->tail = node->prev;
    }
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    if (node->next)
    {
        node->next->prev = node->prev;
    }
    NodeDestroy(node);
}

void NodeDestroy(Node* node)
{
    free(node);
}

void ListDestroy(List* list)
{
    Node* curNode = list->head;
    while (curNode)
    {
        Node* nextNode = curNode->next;
        NodeDestroy(curNode);
        curNode = nextNode;
    }
}
