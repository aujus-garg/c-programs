#include<stdio.h>
#include<string.h>
#include<stdlib.h>

int revBuffer(char* buffer, int start, char delimiter) {
    //Defined Variables
    int end = 0;
    char overwriter;

    //Located End
    while(buffer[end + start + 1] != delimiter &&
          buffer[end + start + 1] != '\0') {
        end++;
    }

    // printf("Length of buffer: %d\n", end);

    //Length of Subset Recorded
    int subset_length = end + 1;

    //Subset Reversed
    for (int i = 0; i < subset_length / 2; i++) {
        overwriter = buffer[start + i];
        // printf("Swapping \"%c\" with \"%c\"\n", overwriter, buffer[start + end - i]);
        buffer[start + i] = buffer[start + end - i];
        buffer[start + end - i] = overwriter;
    }

    return start + subset_length;
}

int tests() {
    {
        char test_string[100];
        strcpy(test_string, "a");
        char* expected = "a";
        int ret = revBuffer(test_string, 0, '\0');
        if (ret != 1) {
            printf("Expected %d, got %d\n", 1, ret);
            return -1;
        }

        if (strcmp(test_string, expected) != 0) {
            printf("Expected %s, found %s\n", expected, test_string);
            return -1;
        }

        // printf("Test passed\n");
    }

    {
        char test_string[100];
        strcpy(test_string, "abcd");
        char* expected = "dcba";
        int ret = revBuffer(test_string, 0, '\0');
        if (ret != 4) {
            printf("Expected %d, got %d\n", 4, ret);
            return -1;
        }

        if (strcmp(test_string, expected) != 0) {
            printf("Expected %s, found %s\n", expected, test_string);
            return -1;
        }

        // printf("Test passed\n");
    }

    {
        char test_string[100];
        strcpy(test_string, "abc");
        char* expected = "cba";
        int ret = revBuffer(test_string, 0, '\0');
        if (ret != 3) {
            printf("Expected %d, got %d\n", 3, ret);
            return -1;
        }

        if (strcmp(test_string, expected) != 0) {
            printf("Expected %s, found %s\n", expected, test_string);
            return -1;
        }

        // printf("Test passed\n");
    }

    {
        char test_string[100];
        char* expected = "lkjih gfe dcba";
        strcpy(test_string, "abcd efg hijkl");
        int ret = revBuffer(test_string, 0, '\0');
        if (ret != 14) {
            printf("Expected %d, got %d\n", 14, ret);
            return -1;
        }

        if (strcmp(test_string, expected) != 0) {
            printf("Expected %s, found %s\n", expected, test_string);
            return -1;
        }

        // printf("Test passed\n");
    }


    return 0;
}

int main() {
    tests();

    //Defined String
    char original_string_one[1000];
    printf("Input a string\n");

    //User Input
    fgets(original_string_one, 1000, stdin);
    strtok(original_string_one, "\n");
    char original_string[strlen(original_string_one) + 1];
    strcpy(original_string_one, original_string);
    free(original_string_one);

    //Reversed Whole String
    revBuffer(original_string, 0, '\0');

    printf("Reversed: %s\n", original_string);

    //Word Count Found
    int word_amount = 0;
    for(int index = 0; original_string[index] != '\0'; index++) {
        if (original_string[index] == ' ') {
            word_amount++;
        }
    }
    word_amount++;

    printf("Found %d words\n", word_amount);

    //Reversed Each Word
    int start = 0;
    for(int i = 0; i < word_amount; i++) {
        start = revBuffer(original_string, start, ' ') + 1;
        printf("[%d][%d] %s\n", i, start, original_string);
    }

    //Printed String
    printf("%s\n", original_string);
}