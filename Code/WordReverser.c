#include <stdio.h>
#include <string.h>
int main()
{
   //Defined Strings
   char origin_string[1000], rev_string[1000];
   printf("Input a string\n");
   //User Input
   fgets(origin_string, 1000, stdin);
   //Defined Variables
   int origin_string_length = strlen(origin_string);
   int new_index = origin_string_length - 1;
   int rev_string_overwriter = 0;
   //Reversed String Made
   for (rev_string_overwriter = 0; rev_string_overwriter < origin_string_length; rev_string_overwriter++) {
      rev_string[rev_string_overwriter] = origin_string[new_index];
      new_index--;
   }
   rev_string[rev_string_overwriter] = '\0';
   printf("%s\n", rev_string);
   return 0;
}