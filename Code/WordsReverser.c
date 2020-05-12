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
   int origin_string_word_count = 0;
   int rev_string_overwriter = 0;
   //Defined Array
   int word_start_index[500];
   word_start_index[0] = 0;
   int array_loc = 1;
   int end_of_string = 0;
   int string_loc = strlen(origin_string);
   //Word Count Found
   for(int index = 0; origin_string[index] >= '\0'; index++) {
       if (origin_string[index] == ' ') {
            word_start_index[array_loc] = index;
            array_loc++;
       }
   }
   int last_word_length = string_loc - word_start_index[array_loc];
   for(int length = 0; length < last_word_length; length++) {
      rev_string[word_start_index[array_loc]] = origin_string[word_start_index[array_loc] + last_word_length];
   }
  /* // word_start_index[string_loc] = '\0';
   //Reversed String Found
   for(int word_loc = string_loc - 1; word_loc >= 0; word_loc--) {
       // printf("%c ", origin_string[word_loc]);
       // printf("%d\n", word_start_index[word_loc]);
       if(origin_string[word_loc] == word_start_index[word_loc]) {
           for(int word_pos = 1 + word_loc; origin_string[word_pos] != '\0' && word_pos != ' '; word_pos++) {
              rev_string[rev_string_overwriter] = origin_string[word_pos];
              rev_string_overwriter++;
           }
       }
   } */
   rev_string[string_loc+1] = '\0';
   printf("%s\n", rev_string);
   return 0;
}