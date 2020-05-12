#include <stdio.h>
 
int a;
 
int main()  {
    printf ("Enter the temperature in Farenheit: ");
    scanf ("%d", &a);
    a = a - 32;
    a = a * 5/9;
    printf ("The temperature in Celsius is %d\n", a);
};