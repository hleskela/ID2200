/*
 *
 * NAME:
 *    minishell - a simple shell for Unix systems which can be run in a terminal.
 *
 * SYNOPSIS:
 *    minishell
 *
 * DESCRIPTION:
 *    Minishell can handle all programs supported under execvp(3) and will run them
 *    accordingly. Native support for foreground and background exists, but no pipe-
 *    lining or redirection of I/O is available. A maximum of 70 chars divided amon-
 *    gst five arguments can be supplied. Built-in's include cd and exit, which act
 *    like cd(3tcl) exit(3tcl) in bash. 
 *
 * EXAMPLES:
 *    'minishell' - runs a shell. For more details regarding shell usage, see e.g.
 *    <http://www.gnu.org/software/bash/manual/bashref.html>
 *
 * ENVIRONMENT:
 *    HOME, PATH (via execvp)
 *
 * SEE ALSO:
 *    bash(1), execvp(3)
 *
 * EXIT STATUS:
 *    0    if OK,
 *    1    can only be returned by a child unable to run execvp(3).
 *
 * AUTHOR:
 *    Written by Hannes A. Leskelä <hleskela@kth.se> and Sam Lööf <saml@kth.se>.
 *
 *    Please send bug reports to <hleskela@kth.se>.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void bgPoll(int*);
void bogus();
void fillWithNull(char**,int);
void forkError(int);
void timeError(int);
void waitError(int);

int main(int argc , char ** argv)
{
  const int INPUT_LIMIT = 72; /* För att fgets räknar med newline och \0. */
  const int MAX_ARGUMENTS = 5; /* Antal argument i varje anrop, inklusive anropet.*/
  char user_input[INPUT_LIMIT]; /* Den råa inmatningen till fgets(). */

  /* Status - variabel som får ta emot returvärden ifrån systemanrop för att sedan checkas av.*/
  int status;
  pid_t childpid;
  struct timeval tv;
  struct timeval tv1;

  /* Allokerar minne en gång, pekar om till null inför varje körning. */
  char **parsed_user_input = malloc((MAX_ARGUMENTS) * sizeof(char *));

  /* Används för att hantera ctrl-c för att inte stänga ned programmet. */
  struct sigaction sigchild;
  memset (&sigchild, '\0', sizeof(sigchild));
  sigchild.sa_handler = bogus;
  sigaction(SIGINT, &sigchild, 0);

  /* Loopen som upprepar inläsning, dvs. själva programmet. */
  for(;;){
    *user_input = '\0'; /* Nollställer user input*/
    printf(">");
    fgets( user_input, INPUT_LIMIT, stdin );
    
    size_t ln = strlen(user_input) - 1;

    if (user_input[ln] == '\n'){
      user_input[ln] = '\0';
    }

    /* 
     * För att kontra segmentation fault om man bara trycker enter.
     * Inga kommandon är 0 stora.
     */
    if(strlen(user_input) < 1){
      continue;
    }

    /* Den behandlade inmatningen via strtok(). */
    char * result;
    result = strtok(user_input," ");
    int num_params = 0;
    
    /* Så att inga rester blir kvar i matrisen från tidigare körning. */
    fillWithNull(parsed_user_input,MAX_ARGUMENTS);

    /* Parsar input och lagrar varje ord i en array av char-pekare.*/
    while(result != NULL && num_params <MAX_ARGUMENTS-1){/* -1 för att vi redan kallat strtok() ovan.*/
      parsed_user_input[num_params] = result;
      num_params++;
      result = strtok(NULL," ");
    }
    
    /* Check ifall exit skrivits.
     * strcmp får ingen errorcheck ty vi kan
     * inte hitta någon informantion om error return.
     */
    status = strcmp(parsed_user_input[0],"exit");
    if (0 == status){
      free(parsed_user_input);
      exit (0);
    }
    
    /* Check ifall cd skrivits. */
    status = strcmp(parsed_user_input[0],"cd");
    if (0 == status){
      status = chdir(parsed_user_input[1]);
      if( -1 == status){
        printf("minishell: cd: %s: %s\n",parsed_user_input[1], strerror(errno));
        /* Om pathen var ogiltig, byt till home definierat i env.var. */
        char * home = getenv("HOME");
        if(home != NULL){
          status = chdir(home);
	  if( -1==status){
	    /* Så att användaren vet vad som händer. */
	    printf("No valid $HOME variable, directory unchanged.\n");
          }
        }
      }
      continue;
    }

    
    /* Pollar bakgrundsprocesser. */
    bgPoll(&status);
    
    /* Check ifall bakgrundsprocess ska startas (& i slutet).*/
    status = strcmp(parsed_user_input[num_params-1],"&");
    if (0 == status){
      parsed_user_input[num_params-1] = '\0';
    }
    
    
    /*Startar tidtagningen.*/
    int temp = gettimeofday(&tv,0);
    timeError(temp);
    
    childpid = fork();
    forkError(childpid);
    if( 0 == childpid){
      
      execvp(parsed_user_input[0],parsed_user_input);
      printf("Could not execute command %s\n",parsed_user_input[0]);
      exit (1); /* Utan denna så skulle barnet köra en onödig wait() */
    }
    if(0 == status){
      printf("\nSpawned background process pid: %i\n",childpid);
      continue;
    }
    wait (&status);
    waitError(status);
    /*Stoppar tidtagningen.*/
    temp = gettimeofday(&tv1,0);
    timeError(temp);
    /*  Man kan inte enkom jämföra µsek, utan sekunderna måste tas med.
     *  Det är som att ange tid i bara minuter mellan 0-60, om det har gått
     *  en viss tid kan man få negativa tidsåtgångar, och man får aldrig ett
     *  värde större än 59 min.*/
    float time = ((tv1.tv_sec - tv.tv_sec) * 1000.0)
      + ((tv1.tv_usec - tv.tv_usec) / 1000.0);
    printf("\nSpawned foreground process pid: %i\n",childpid);
    printf("Foreground process %i terminated\n",childpid);
    printf("Wallclock time: %.2f ms\n",time);
    
  }
  
  /* Väluppfostrade mallocs städar efter sig. */
  free(parsed_user_input);
  
  return 0;
}


/* bgPoll
 *
 * bgPoll returns nothing, but loops through any receiving
 * signals from background processes and waits for them 
 * without blocking the main process.
 *
 * @param    int * status
 */
void bgPoll(int * status){
  int value =waitpid(-1, status, WNOHANG);
  while(value >0){
    if(WIFEXITED(*status)){
      printf("Background process %i terminated\n",value);
    }
    value = waitpid(-1, status, WNOHANG);
  }
}


/* bogus
 *
 * bogus returns nothing and is the handler for incoming
 * SIGINT for the main() process.
 */
void bogus(){
  printf("\n");
}



/* fillWithNull
 *
 * fillWithNull returns nothing, but loops through a
 * char matrix and fills it with NULL pointers. This
 * is to ensure that no trailing char pointers are
 * left in the matrix parsed_user_input in the main()
 * function.
 *
 * @param    char ** matrix
 * @param    int limit
 */
void fillWithNull(char ** matrix,int limit){
  int j;
  for(j=0;j<limit;j+=1){
    matrix[j]=NULL;
  }
}


/* forkError
 *
 * forkError returns nothing and is only meant to exit a process in a
 * controlled mannor.
 *
 * @param    int errorCode
 */
void forkError(int errorCode)
{ 
  if( -1 == errorCode ){
    printf("Cannot fork process.\n%s\n",strerror(errno));
  }
}


/* timeError
 *
 * timeError returns nothing but prints the errno message to STDOUT
 * to keep the user updated.
 *
 * @param    int errorCode
 */
void timeError(int errorCode)
{
  if( -1 == errorCode ){
    printf("Error signal recieved from child process.\n%s\n",strerror(errno));
  }
}

/* waitError
 *
 * waitError returns nothing but prints the errno message to STDOUT
 * to keep the user updated.
 *
 * @param    int errorCode
 */
void waitError(int errorCode)
{
  if( -1 == errorCode ){
    printf("Error signal recieved from child process.\n%s\n",strerror(errno));
  }
}
