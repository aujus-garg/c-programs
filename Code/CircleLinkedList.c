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

void insert_node(int value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    //printf("Inserting %d\n", value);
    new_node->value = value;
    if(pointer == NULL) { 
        pointer = new_node;
        new_node->next = pointer;
        new_node->previous = pointer;
    } else {
        struct node *next_pointer = pointer->next;

        // setup the new node
        new_node->next = next_pointer;
        next_pointer->previous = new_node;

        // Update pointer
        pointer->next = new_node;
        new_node->previous = pointer;

        // move pointer to the new node
        pointer = new_node;
    }
}

void sort_insert_ascending(int value) {
    struct node *current = pointer;
    if (current == NULL) {
        insert_node(value);
        return;
    }
    if (current->value > value) {
        struct node *new_node = (struct node*)malloc(sizeof(struct node));
        new_node->value = value;
        new_node->previous = current->previous;
        current->previous->next = new_node;
        current->previous = new_node;
        new_node->next = current;
        pointer = new_node;
        return;
    }

    if (current->value == value) {
        return;
    }

    while(current != NULL) {
        if (current->next == pointer || current->next->value > value) {
            struct node *new_node = (struct node*)malloc(sizeof(struct node));
            new_node->value = value;
            new_node->next = current->next;
            current->next->previous = new_node; 
            new_node->previous = current;
            current->next = new_node;
            return;
        }

        if (current->next->value == value || current->next == pointer) {
            return;
        }
        current = current->next;
    }
}

void sort_insert_descending(int value) {
    struct node *current = pointer;
    if (current == NULL || current->value < value) {
        insert_node(value);
        return;
    }

    if (current->value == value) {
        return;
    }

    while (current != NULL) {
        if (current->next == NULL || current->next->value < value) {
            struct node *new_node = (struct node*)malloc(sizeof(struct node));
            new_node->value = value;
            new_node->next = current->next;
            new_node->previous = current;
            current->next = new_node;
            return;
        }

        if (current->next->value == value) {
            return;
        }
        current = current->next;
    }
}

void remove_node(int value) {
    struct node *current = pointer;
    while(current != NULL && current->value == value) {
        current = pointer->next;
        current->previous = NULL;
        free(pointer);
        pointer = current;
    }
    while(current != NULL) {
        if (current->value == value) {
            struct node *free_node = current;
            current->next->previous = current->previous;
            current->previous->next = current->next;
            free(free_node);
        }
        if(current->next == pointer) {
            return;
        }
        current = current->next;
    }
}

void traverse_node(bool forward) {
    struct node *current = pointer;
    if(forward) {
       while(current != NULL) {
            current = current->previous;
            printf("%d\n", current->value);
            if(current == pointer) {
                return;
            }
       } 
    } else {
        while(current != NULL) {
            printf("%d\n", current->value);
            if(current == pointer->previous) {
                return;
            }
            current = current->next;
        }
    }
}

int main() {
    sort_insert_ascending(4);
    sort_insert_ascending(6);
    sort_insert_ascending(3);
    sort_insert_ascending(2);
    traverse_node(false);
}