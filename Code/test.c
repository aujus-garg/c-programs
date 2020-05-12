#include <stdio.h>
#include <stdlib.h>

int main() {
    int i = 100;
    int *pointer = NULL;
    pointer = &i;
    printf("The value pointer points to is %d\n", *pointer);
    *pointer = 200;
    printf("Pointer now points to %d\n", *pointer);
}