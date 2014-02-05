/* strtok example */
#include <stdio.h>
#include <string.h>

int main ()
{
  char strbuf[] ="- This, a sample string.";
  char *str=strbuf;
  char * pch;
  printf ("Splitting string \"%s\" into tokens:",str);
  while ((pch=strtok(str," ,.-")) != NULL) {
    printf ("%s",pch); str=NULL;
  }
  return 0;
}
