#include <stdlib.h>
#include <stdio.h>

#define SIZE 10
#define TIMES 1000

int main(int argc, char *argv[]){
  int i;
  char *array[TIMES];
  for(i=0;i<TIMES;i++)
    array[i] = malloc(1024*1024);

  for(i=0;i<TIMES;i++)
    free(array[i]);

  return 0;
}



