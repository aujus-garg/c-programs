#include <stdio.h>
void numFunction(int first_num, int second_num, int *sum, int *product) {
    *sum = first_num + second_num;
    *product = first_num * second_num;
}
int main() {
    //Variables
    int num_one;
    int num_two;
    int s = 0;
    int p = 0;
    //User Input
    printf("Enter your first number:\n");
    scanf("%d", &num_one);
    printf("Enter your second number:\n");
    scanf("%d", &num_two);
    //Printed Function
    numFunction(num_one, num_two, &s, &p);
    printf("The sum of these two numbers is %d, and the product of these two numbers is %d\n", s, p);
}