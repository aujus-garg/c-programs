#include <stdio.h>
#include <string.h>
int main()
{
   //Defined Strings
   char origin_string[1000], new_string[1000];
   //Defined Variables
   int array_loc = 0;
   //Defined Array
   int word_start_index[1000];
   printf("Input a string\n");
   //String Input
   fgets(origin_string, 1000, stdin);
   //Word Count Found
      for(int index = 0; origin_string[index] >= '\0'; index++) {
       if (origin_string[index] == ' ') {
            word_start_index[array_loc] = index;
            array_loc++;
       }
   }
   //Subset Found
   int subset = 0;
   int end_of_word = strlen(origin_string) - 1;
   for (int i = 1; i <= array_loc + 1; i++) {
      for(int index_original = 0; index_original < end_of_word - word_start_index[array_loc - i]; index_original++, subset++) {
         new_string[subset] = origin_string[index_original + word_start_index[array_loc - i]];
         //printf("%d, %d, new_string[%d] = %c\n", index_original, i, subset, origin_string[index_original + word_start_index[array_loc - i]]);
      }
      new_string[subset++] = ' ';
      end_of_word = word_start_index[array_loc - i];
   }
   new_string[subset] = '\0';
   printf("%s\n", new_string);
}