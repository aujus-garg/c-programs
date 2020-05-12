#include <stdio.h>
#include <string.h>
void my_strrev(char *original_string, int original_string_length, int end_indexes, int string_overwriters, char start_letter) {
   int string_overwriter = string_overwriters;
   int end_index = end_indexes;
   //Reversed String Made
   for (; string_overwriter < original_string_length / 2; string_overwriter++, end_index--) {
      start_letter = original_string[string_overwriter];
      original_string[string_overwriter] = original_string[end_index];
      original_string[end_index] = start_letter;
   }
}
int main() {
   //Defined String
   char original_string[1000];
   printf("Input a string\n");
   //User Input
   fgets(original_string, 1000, stdin);
   //Defined Variables
   int original_string_length = strlen(original_string);
   int end_index = original_string_length - 1;
   char start_letter;
   int array_loc = 1;
   //Defined Array
   int word_start_index[500];
   word_start_index[0] = 0;
   //Word Count Found
   for(int index = 0; original_string[index] != '\0'; index++) {
      if (original_string[index] == ' ') {
         word_start_index[array_loc] = index + 1;
         array_loc++;
      }
   }
   word_start_index[array_loc] = original_string_length;
   word_start_index[array_loc + 1] = '\0';
   for(int loop = 0; loop < 10; loop++)
      printf("%d ", word_start_index[loop]);
   //Reversed String Made
   my_strrev(original_string, original_string_length, end_index, 0, start_letter);
   for(int i = 0; i < array_loc; i++) {
       my_strrev(original_string, word_start_index[i + 1] - word_start_index[i], word_start_index[i + 1] - 1, word_start_index[i], start_letter);
   }
   //String Printed
   printf("%s\n", original_string);
}