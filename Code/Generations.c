#include <stdio.h>    
 
int main()                            
{
    int age;                       
   
    printf( "Please enter your birth year\n");  
    scanf( "%d", &age);               
    if ( age >= 2013) {                
        printf ("You are a bab- I mean born in an unfininshed generation\n");
    }
    else if ( age >= 1995) {  
        printf( "U R k3wl, f3ll0w Z00m3r\n");       
    }
    else if (age >= 1980) {
        printf( "Millenials, amirite\n");
    } 
    else if (age >= 1965){
        printf("GenX\n");
    }
    else 
        printf("OK BOOMER\n")
    ;
  return 0;
}