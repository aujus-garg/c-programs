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

struct node *pointer_one = NULL;
struct node *pointer_two = NULL;

void insert_node(int value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    //printf("Inserting %d\n", value);
    new_node->value = value;
    if(pointer_one == NULL) {
        pointer_one = new_node;
    }
    if(pointer_two == NULL) {
        pointer_two = new_node;
    }
    new_node->next = pointer_one;
    pointer_one->previous = new_node;
    pointer_one = new_node;
    pointer_two->next = pointer_one;
    pointer_one->previous = pointer_two;
}

void sort_insert_ascending(int value) {
    struct node *current = pointer_one;
    if (current == NULL || current->value > value) {
        insert_node(value);
        return;
    }

    if (current->value == value) {
        return;
    }

    while(current != NULL) {
        if (current->next == pointer_one || current->next->value > value) {
            struct node *new_node = (struct node*)malloc(sizeof(struct node));
            new_node->value = value;
            new_node->next = current->next;
            new_node->previous = current;
            current->next = new_node;
            if(new_node->previous == pointer_two) {
                pointer_two = new_node;
            }
        }

        if (current->next->value == value) {
        return;
        }
        if (current->next == pointer_one) { 
            return;
        }
        current = current->next;
    }
}

void sort_insert_descending(int value) {
    struct node *current = pointer_one;
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
    struct node *current = pointer_one;
    while(current != NULL && current->value == value) {
        current = pointer_one->next;
        current->previous = NULL;
        free(pointer_one);
        pointer_one = current;
    }
    while(current != NULL) {
        if (current->value == value) {
            struct node *free_node = current;
            if(pointer_two == free_node) {
                pointer_two = pointer_two->previous;
            }
            current->next->previous = current->previous;
            current->previous->next = current->next;
            free(free_node);
        }
        if(current->next == pointer_one) {
            return;
        }
        current = current->next;
    }
}

void traverse_node(bool forward) {
    struct node *current = pointer_one;
    if(forward) {
        while(current != NULL) {
            printf("%d\n", current->value);
            if(current == pointer_two) {
                return;
            }
            current = current->next;
        }
    } else {
        while(current != NULL) {
            current = current->previous;
            printf("%d\n", current->value);
            if(current->previous == pointer_two) {
                return;
            }
        }
    }
}

int main() {
    sort_insert_ascending(10);
    sort_insert_ascending(10);
    sort_insert_ascending(1);
    sort_insert_ascending(21);
    sort_insert_ascending(10);
    sort_insert_ascending(3);
//    traverse_node(false);
    printf("\n");
//    remove_node(10);
//    traverse_node(true);
}