#include<stdio.h>
#include<stdlib.h>

struct record;

struct record {
    void *pntr;
    struct record *next;
};

struct record *head = NULL;

void push(void *pntr) {
    struct record *new_record = (struct record*)malloc(sizeof(struct record));
    new_record->pntr = pntr;
    new_record->next = head;
    head = new_record;
}

void * pop() {
    void *return_pntr = NULL;
    if(head != NULL) {
        struct record *next = head->next;
        return_pntr = head->pntr;
        free(head);
        head = next;
    }
    return return_pntr;
}