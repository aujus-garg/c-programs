#include<stdio.h>
#include<stdlib.h>

struct node;

struct node {
    int value;
    struct node *next;
};

struct node *head = NULL;

void insert_node(int value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    //printf("Inserting %d\n", value);
    new_node->value = value;
    new_node->next = head;
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

    while (current != NULL) {
        if (current->next == NULL || current->next->value > value) {
            struct node *new_node = (struct node*)malloc(sizeof(struct node));
            new_node->value = value;
            new_node->next = current->next;
            current->next = new_node;
            return;
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
        free(head);
        head = current;
    }
    while(current != NULL) {
        if (current->next != NULL) {
            if (current->next->value == value) {
                struct node *free_node = current->next;
                current->next = current->next->next;
                free(free_node);
            }
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
    sort_insert_descending(35);
    sort_insert_descending(1);
    sort_insert_descending(334);
    sort_insert_descending(124);
    sort_insert_descending(7790);
    sort_insert_descending(2);
    sort_insert_descending(44);
    sort_insert_descending(545);
    sort_insert_descending(234);

    traverse_node();

    remove_node(124);
    traverse_node();

    remove_node(125);
    traverse_node();

    remove_node(8000);
    traverse_node();
}