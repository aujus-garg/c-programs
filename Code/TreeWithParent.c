#include<stdio.h>
#include<stdlib.h>
#include"Stack.h"

struct node;

struct node {
    int value;
    struct node *left;
    struct node *right;
    struct node *parent;
};

struct node *root = NULL;

void insert(value) {
    struct node *new_node = (struct node*)malloc(sizeof(struct node));
    struct node *current = root;
    new_node->value = value; 
    new_node->right = NULL;
    new_node->left = NULL;
    new_node->parent = NULL;
    if(root == NULL) {
        root = new_node;
        return; 
    }
    while(current != NULL) {
        if(current->value > value) {
            if(current->left == NULL) {
                current->left = new_node;
                new_node->parent = current;
                return;
            } else {
                current = current->left;
            }
        } else if(current->value < value) {
            if(current->right == NULL) {
                current->right = new_node;
                new_node->parent = current;
                return;
            } else {
                current = current->right;
            }
        } else {
            free(new_node);
            return;
        }
    }
}

struct node* traverse_left_subtree(struct node* subtree_root) {
    while(subtree_root->left != NULL) {
       subtree_root = subtree_root->left;
    }
    return subtree_root;
}


void traverse_inorder() {
    struct node *current = root;
    struct node *end = root;
    while(end->right != NULL) {
        end = end->right;
    }
    current = traverse_left_subtree(current);
    while(1) {
        printf("%d\n", current->value);
        if(current == end) {
            return;
        } else if(current->right != NULL) {
            current = current->right;
            current = traverse_left_subtree(current);
        } else {
            while(current->parent->right == current) {
                current = current->parent;
            }
            current = current->parent;
        }
    }
}

void traverse_recursive_inorder(struct node* root_node) {
    if (root_node != NULL) {
        traverse_recursive_inorder(root_node->left);
        printf("%d\n", root_node->value);
        traverse_recursive_inorder(root_node->right);
    }
}

int main() {
    insert(10);
    insert(15);
    insert(3);
    insert(5);
    insert(17);
    insert(13);
    insert(7);
    insert(4);
    insert(1);
    insert(200);
    insert(30);
    insert(12);
    insert(49);
    insert(24);
    insert(0);
    insert(400);
    insert(9);
    traverse_inorder();
    printf("\n\n");
    insert(2);
    traverse_inorder();
}