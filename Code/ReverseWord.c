#include <stdio.h>
#include <string.h>
int main()
{
   //Defined String
   char original_string[1000];
   printf("Input a string\n");
   //User Input
   fgets(original_string, 1000, stdin);
   //Defined Variables
   int original_string_length = strlen(original_string);
   int end_index = original_string_length - 1;
   int string_overwriter = 0;
   char start_letter;
   //Reversed String Made
   for (; string_overwriter < original_string_length / 2; string_overwriter++, end_index--) {
      start_letter = original_string[string_overwriter];
      original_string[string_overwriter] = original_string[end_index];
      original_string[end_index] = start_letter;
   }
   //Reversed String Printed
   original_string[original_string_length + 1] = '\0';
   printf("%s\n", original_string);
   return 0;
}