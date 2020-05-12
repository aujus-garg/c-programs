#include<stdio.h>
#include<stdlib.h>

struct node;

struct node {
    int value;
    struct node *next;
    struct node *previous;
};

struct node *head = NULL;

void insert_node(int value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    //printf("Inserting %d\n", value);
    new_node->value = value;
    new_node->next = head;
    new_node->previous = NULL;
    if(head != NULL) {
        head->previous = new_node;
    }
    head = new_node;
}

void sort_insert_ascending(int value) {
    struct node *current = head;
    if (current == NULL || current->value > value) {
        insert_node(value);
        return;
    }

    if (current->value == value) {
        return;
    }

    while(current != NULL) {
        if (current->next == NULL || current->next->value > value) {
            struct node *new_node = (struct node*)malloc(sizeof(struct node));
            new_node->value = value;
            new_node->next = current->next;
            new_node->previous = current;
            current->next = new_node;
        }

        if (current->next->value == value) {
        return;
        }

        current = current->next;
    }
}

void sort_insert_descending(int value) {
    struct node *current = head;
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
    struct node *current = head;
    while(current != NULL && current->value == value) {
        current = head->next;
        current->previous = NULL;
        free(head);
        head = current;
    }
    while(current != NULL) {
        if (current->value == value) {
            struct node *free_node = current;
            if (current->next != NULL) {
                current->next->previous = current->previous;
            }
            current->previous->next = current->next;
            free(free_node);
        }
        current = current->next;
    }
}

void traverse_node() {
    struct node *current = head;
    while(current != NULL) {
        printf("%d\n", current->value);
        current = current->next;
    }
}

int main() {
    sort_insert_descending(10);
    sort_insert_descending(10);
    sort_insert_descending(1);
    sort_insert_descending(21);
    sort_insert_descending(10);
    sort_insert_descending(3);
    traverse_node();
    sort_insert_descending(5);
    printf("\n");
    remove_node(21);
    traverse_node();
    printf("\n");
    remove_node(10);
    traverse_node();
    printf("\n");
    remove_node(5);
    traverse_node();
    printf("\n");
}