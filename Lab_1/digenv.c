/*
 *
 * NAME:
 *    digenv - a program that formats and print the output from printenv to a pager.
 *
 * SYNOPSIS:
 *    digenv [OPTIONS] PATTERN
 *    digenv [OPTIONS] [-e PATTERN]
 *
 * DESCRIPTION:
 *    Digenv sends the output from the printenv() user command through a predefined
 *    formatter on to a pager. If no arguments are supplied to digenv, the default
 *    route is via sort and then the $PAGER of the system. If no $PAGER variable is
 *    set, the default is less. If arguments are supplied, the printenv output is 
 *    sent to grep to search for a specified pattern in accordance with grep's man 
 *    page. 
 *
 * OPTIONS:
 *    See man grep(1) or info coreutils 'grep invocation'
 *
 * EXAMPLES:
 *    'digenv -e PATH -e USER' - Displays all occurrences of *PATH* and *USER* from
 *    the printenv user command, sorted, in your default pager.
 *
 *    'digenv' - Displays the printenv user command, sorted, in your default pager.
 *
 * ENVIRONMENT:
 *    PAGER
 *
 * SEE ALSO:
 *    grep(1), less(1), more(1), printenv(1)
 *
 * EXIT STATUS:
 *    0    if OK,
 *    1    could not create pipe,
 *    2    could not fork parent process,
 *    3    could not duplicate file descriptor table with dup2,
 *    4    could not execute printenv(1) via execlp,
 *    5    could not execute grep(1) via execvp,
 *    6    could not execute sort(1) via execlp,
 *    7    could not execute pager command via execlp,
 *    8    error signal from awaited child process,
 *    9    could not close a pipe end.
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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PIPE_READ ( 0 )
#define PIPE_WRITE ( 1 )

void closeError(int);
void dup2Error(int);
void forkError(int);
void pipeError(int);
void waitError(int);

pid_t childpid; /* För koll av child/parent-processens PID vid fork(). */

/* main
 * 
 * main returns 0 if successfull and displays the result on STDOUT.
 *
 */
int main(int argc, char **argv)
{
 
  int return_value; /* För returvärden där error check är det viktiga. */
  int status; /* För att kontrollera status vid inväntande utav child-processer*/
  int pfd_printenv_sort[ 2 ]; /* För returvärdet från pipe mellan printenv och sort. */
  
  return_value = pipe( pfd_printenv_sort ); /* Pipe mellan printenv och sort. */
  pipeError(return_value);

  childpid = fork();
  forkError(childpid);

  /* Ifall fork() lyckades skall barnet utföra if-satsen. */
  if( 0 == childpid ){
    
    return_value = dup2( pfd_printenv_sort[ PIPE_WRITE ],
			 STDOUT_FILENO ); /* STDOUT_FILENO == 1. */
    dup2Error(return_value);
    
    return_value = close( pfd_printenv_sort[ PIPE_READ ]);
    closeError(return_value);
    return_value = close( pfd_printenv_sort[ PIPE_WRITE ] );
    closeError(return_value);
    /*Utför processen "printenv" istället för att 
      fortsätta i denna process. */
    execlp("printenv","printenv",(void *) 0);
    perror("Could not execute command printenv. ");
    exit ( 4 );
  }



  /* Detta skall köras endast om parametrar bifogas. 
     Då skall dessa skickas med till grep.*/
  int pfd_grep_sort[ 2 ];
  /* Lista för parametrar till grep. */
  char **arg_list = malloc((argc + 1) * sizeof(char *));
  if(argc>1){
    return_value = pipe( pfd_grep_sort );
    pipeError( return_value );

    childpid = fork();
    forkError( childpid );

    if( 0 == childpid ){
      int counter;
      arg_list[0] = "grep";
      /*Loopar in alla argument i en array för att sedan köra
	execvp med dessa argument.*/
      for(counter=1;counter<argc;counter++){
	arg_list[counter] = argv[counter];
      }
      /* Måste avsluta med \0. */
      arg_list[argc] = NULL;

      return_value = dup2( pfd_printenv_sort[ PIPE_READ ],
			   STDIN_FILENO );
      dup2Error(return_value);
      
      return_value = dup2( pfd_grep_sort[ PIPE_WRITE ],
			   STDOUT_FILENO );
      dup2Error(return_value);

      return_value = close(pfd_grep_sort[ PIPE_READ ]);
      closeError(return_value);
      return_value = close(pfd_grep_sort[ PIPE_WRITE ]);
      closeError(return_value);
      return_value = close(pfd_printenv_sort[ PIPE_READ]);
      closeError(return_value);
      return_value = close(pfd_printenv_sort[ PIPE_WRITE]);
      closeError(return_value);
      
      (void) execvp(arg_list[0],arg_list);
      perror("Could not execute command grep. ");
      exit ( 5 ); 
    }
  }
  
  /* Pipe mellan sort och less/more */
  int pfd_sort_less[ 2 ];
  return_value = pipe( pfd_sort_less );

  /* Ny forkning för körning av sort. */
  childpid = fork();
  forkError(childpid);
  
  if( 0 == childpid ){
    
    
    /* Väljer rätt pipe beroende på om grep kördes eller ej. */
    if( argc >1){
      return_value = dup2( pfd_grep_sort[ PIPE_READ ],
			   STDIN_FILENO ); /* STDIN_FILENO == 0. */
    }else{
    return_value = dup2( pfd_printenv_sort[ PIPE_READ ],
			 STDIN_FILENO ); /* STDIN_FILENO == 0. */
    }
    dup2Error(return_value);
    
    return_value = dup2( pfd_sort_less[ PIPE_WRITE ],
			 STDOUT_FILENO );
    dup2Error( return_value );
    
    /* Stänger de onödiga pipe-fdt:erna*/
    return_value = close( pfd_sort_less[ PIPE_WRITE ]);
    closeError(return_value);
    return_value = close( pfd_sort_less[ PIPE_READ ]);
    closeError(return_value);
    if(argc >1){/* Ifall att pipen skapades för körning utav grep. */
      return_value = close( pfd_grep_sort[ PIPE_WRITE ]);
      closeError(return_value);
      return_value = close( pfd_grep_sort[ PIPE_READ ]);
      closeError(return_value);
    }
    return_value = close( pfd_printenv_sort[ PIPE_WRITE ]);
    closeError(return_value);
    return_value = close( pfd_printenv_sort[ PIPE_READ ]);
    closeError(return_value);

    (void) execlp("sort","sort",(void *) 0);    
    perror("Could not execute command sort. ");
    exit ( 6 );
    
  }

  /* Stänger pipes som ej kommer användas mer. */

  if(argc >1){
    return_value = close( pfd_grep_sort[ PIPE_WRITE ]);
    closeError(return_value);
    return_value = close( pfd_grep_sort[ PIPE_READ ]);
    closeError(return_value);
  }

  return_value = close( pfd_printenv_sort[ PIPE_WRITE ]);
  closeError(return_value);
  return_value = close( pfd_printenv_sort[ PIPE_READ ]);
  closeError(return_value);

  /* Ny forkning för körning utav less/more. */
  childpid = fork();
  forkError(childpid);
  
  if( 0 == childpid){
    
    char * pager = getenv("PAGER");

    return_value = dup2( pfd_sort_less[ PIPE_READ ],
			 STDIN_FILENO );

    dup2Error( return_value );
    
    return_value = close( pfd_sort_less[ PIPE_READ ] );
    closeError(return_value);
    return_value = close( pfd_sort_less[ PIPE_WRITE ] );
    closeError(return_value);

    if( pager != NULL ){
      (void) execlp(pager,pager,(void *) 0);
      perror("Could not execute default pager."); 
      exit ( 8 );
    }
    (void) execlp("less","less",(void *) 0);
    perror("Could not execute command less. ");
    exit ( 7 );
  }

  /*Stängning utav föräldraprocessens pfd's. */
  return_value = close( pfd_sort_less[ PIPE_READ ]);
  closeError(return_value);
  return_value = close( pfd_sort_less[ PIPE_WRITE ]);
  closeError(return_value);

  /* Dessa krävs för att föräldraprocessen ska vänta in exit status
     för alla dess barn. En wait() per fork(). */
  childpid = wait( &status );
  waitError(childpid);
  childpid = wait( &status );
  waitError(childpid);
  childpid = wait( &status );
  waitError(childpid);
  if( argc>1){/* Ifall grep-processen existerar. */
    childpid = wait( &status );
    waitError(childpid);
  }

  /* Väluppfostrade program städar efter sig. */
  free(arg_list);
  return 0;
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
    perror("Cannot fork process.");
    exit( 2 );
  }
}

/* dup2Error
 *
 * dup2Error returns nothing and is only meant to exit a process in a
 * controlled mannor.
 *
 * @param    int errorCode
 */
void dup2Error(int errorCode)
{
  if ( -1 == errorCode ){
    perror("Could not duplicate file descriptor table.");
    exit( 3 );
  } 
}

/* pipeError
 *
 * pipeError returns nothing and is only meant to exit a process in a
 * controlled mannor.
 *
 * @param    int errorCode
 */
void pipeError(int errorCode)
{
  if( -1 == errorCode ){
    perror("Cannot create pipe.");
    exit( 1 );
  }
}

/* waitError
 *
 * waitError returns nothing and is only meant to exit a process in a
 * controlled mannor.
 *
 * @param    int errorCode
 */
void waitError(int errorCode)
{
  if( -1 == errorCode ){
    perror("Error signal recieved from child process.");
    exit( 8 );
  }
}

/* closeError
 *
 * closeError returns nothing and is only meant to exit a process in a
 * controlled mannor.
 *
 * @param    int errorCode
 */
void closeError(int errorCode)
{
  if( -1 == errorCode ){
    perror("Could not close pipe.");
    exit( 9 );
  }
}
