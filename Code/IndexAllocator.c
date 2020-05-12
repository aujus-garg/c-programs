#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>

struct node;

struct node {
    int value;
    struct node *next;
    struct node *previous;
};

struct node *pointer = NULL;

void free_value(int value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    new_node->value = value;
    if(pointer == NULL) {
        pointer = new_node;
        new_node->next = pointer;
        new_node->previous = pointer;
    } else {
        pointer->previous->next = new_node;
        new_node->previous = pointer->previous;
        new_node->next = pointer;
        pointer->previous = new_node;
        pointer = new_node;
    }
}

void initialize(int start, int end) {
    int value = start;
    for(; value <= end; value++) {
        struct node *new_node = (struct node*)malloc(sizeof(struct node));
        new_node->value = value;
        if(pointer == NULL) { 
            pointer = new_node;
            new_node->next = pointer;
            new_node->previous = pointer;
        } else {
            new_node->previous = pointer->previous;
            pointer->previous->next = new_node;
            pointer->previous = new_node;
            new_node->next = pointer;
            pointer = new_node;
        }
    }
}

int allocate() {
    if(pointer != NULL) {
        struct node *free_node = pointer;
        int value = pointer->value;
        pointer->next->previous = pointer->previous;
        pointer->previous->next = pointer->next;
        if(pointer->next != pointer) {
            pointer = pointer->next;
        } else {
            pointer = NULL;
        }
        free(free_node);
        return value;
    } else {
        return -1;
    }
}

void traverse_node(bool forward) {
    struct node *current = pointer;
    if(forward) {
        while(current != NULL) {
            printf("%d\n", current->value);
            if(current == pointer->previous) {
                return;
            }
            current = current->next;
        }   
    } else {
        while(current != NULL) {
            current = current->previous;
            printf("%d\n", current->value);
            if(current == pointer) {
                return;
            }
       } 
    }
}

int main() {
    initialize(0, 99);
    for(int i = 0; i < 100; i++) {
        printf("Allocated %d\n", allocate());
    }
    traverse_node(true);
    printf("\n");
    for(int i = 99; i >= 0; i--) {
        free_value(i);
    }

    allocate();
    allocate();
    allocate();
    allocate();
    allocate();
    allocate();
    traverse_node(false);

    free_value(3);
    free_value(4);
    for(int i = 0; i < 10; i++) {
        printf("Allocated %d\n", allocate());
    }
}