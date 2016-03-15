/*
 *
 * NAME:
 *    malloc.c - a memory manager for Unix-like systems, tested on Ubuntu 12.04.
 *
 * SYNOPSIS:
 *    malloc (size_t nbytes)
 *    realloc (void *ptr, size_t size)
 *    free (void *ap)
 *
 *    Consider 'gcc -DStrategy=[1,3] malloc.c' for different memory allocation methods.
 *
 * DESCRIPTION:
 *    Malloc is a dynamic memory manager for Unix-like systems and includes the functions
 *    malloc, realloc and free. The original code stems from K&R, see author for reference.
 *    Contributions include a realloc and two different memory allocation methods, First Fit
 *    and Worst Fit. The default algortithm is First Fit but you can also explicitly choose
 *    an algorithm via the compilation flag -D with STRATEGY assigned either:
 *
 *    1 , which is the default First Fit, or
 *    3 , the Worst Fit algorithm.
 *
 * EXAMPLES:
 *    char *p;
 *    p = malloc(17);
 *    if (p == NULL)
 *       exit (1);
 *    free(p);
 *
 * SEE ALSO:
 *    malloc(3)
 *
 * EXIT STATUS:
 *    0
 *
 * AUTHOR:
 *    Written by Brian Kernighan and Dennis Ritchie, see 'http://cm.bell-labs.com/cm/cs/cbook/'.
 *    Edited by Hannes A. Leskelä <hleskela@kth.se> and Sam Lööf <saml@kth.se>.
 *
 *    Please send bug reports to <hleskela@kth.se>.
 *
 */



#include "brk.h"
#include <unistd.h>
#include <string.h> 
#include <errno.h> 
#include <sys/mman.h>
#include <stdio.h>

#define NALLOC 1024                                    /* minimum #units to request */

typedef long Align;                                     /* for alignment to long boundary */

union header {                                          /* block header */
  struct {
    union header *ptr;                                  /* next block if on free list */
    unsigned size;                                      /* size of this block  - what unit? */ 
  } s;
  Align x;                                              /* force alignment of blocks */
};

typedef union header Header;

static Header base;                                     /* empty list to get started */
static Header *freep = NULL;                            /* start of free list */

/* free
 *
 * free returns nothing, it simply frees memory allocated by malloc.
 *
 * @param    void * ap
 */
void free(void * ap)
{
  Header *bp, *p;

  if(ap == NULL) return;                                /* Nothing to do */

  bp = (Header *) ap - 1;                               /* point to block header */
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;                                            /* freed block at atrt or end of arena */
  
  if(bp + bp->s.size == p->s.ptr) {                     /* join to upper nb */
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  }
  else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp) {                             /* join to lower nbr */
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

/* morecore: ask system for more memory */

#ifdef MMAP

static void * __endHeap = 0;

void * endHeap(void)
{
  if(__endHeap == 0) __endHeap = sbrk(0);
  return __endHeap;
}
#endif


static Header *morecore(unsigned nu)
{
  void *cp;
  Header *up;
#ifdef MMAP
  unsigned noPages;
  if(__endHeap == 0) __endHeap = sbrk(0);
#endif

  if(nu < NALLOC)
    nu = NALLOC;
#ifdef MMAP
  noPages = ((nu*sizeof(Header))-1)/getpagesize() + 1;
  cp = mmap(__endHeap, noPages*getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  nu = (noPages*getpagesize())/sizeof(Header);
  __endHeap += noPages*getpagesize();
#else
  cp = sbrk(nu*sizeof(Header));
#endif
  if(cp == (void *) -1){                                 /* no space at all */
    perror("failed to get more memory");
    return NULL;
  }
  up = (Header *) cp;
  up->s.size = nu;
  free((void *)(up+1));
  return freep;
}


/* malloc
 *
 * malloc returns a pointer to the allocated area. 
 * If STRATEGY is defined the execution path may vary
 * depending on the value of the variable.
 *
 * @param    size_t nbytes
 */
void * malloc(size_t nbytes)
{

  Header *p, *prevp;
  Header * morecore(unsigned);
  unsigned nunits;

  if(nbytes <= 0) return NULL;

  nunits = (nbytes+sizeof(Header)-1)/sizeof(Header) +1;

  if((prevp = freep) == NULL) {
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  /* Worst Fit*/
#ifdef STRATEGY
  if(STRATEGY == 3){
    Header * biggest = base.s.ptr;
    for(p=biggest->s.ptr;p !=base.s.ptr  ; prevp = p, p = p->s.ptr) {
      if(p->s.size>biggest->s.size){
	biggest = p;
      }
    }
    if(biggest->s.size >= nunits) {                           /* big enough */
      if (biggest->s.size == nunits)                          /* exactly */
	prevp->s.ptr = biggest->s.ptr;
      else {                                            /* allocate tail end */
	biggest->s.size -= nunits;
	biggest += biggest->s.size;
	biggest->s.size = nunits;
      }
      freep = prevp;
      return (void *)(biggest+1);
    }
    if(biggest->s.size < nunits)                                      /* wrapped around free list */
      if((biggest = morecore(nunits)) == NULL)
	return NULL;                                    /* none left */
  }
#endif

  /*First Fit - Default*/
  prevp = &base;
  for(p=prevp->s.ptr;  ; prevp = p, p = p->s.ptr) {
    if(p->s.size >= nunits) {                           /* big enough */
      if (p->s.size == nunits)                          /* exactly */
	prevp->s.ptr = p->s.ptr;
      else {                                            /* allocate tail end */
	p->s.size -= nunits;
	p += p->s.size;
	p->s.size = nunits;
      }
      freep = prevp;
      return (void *)(p+1);
    }
    if(p == freep)                                      /* wrapped around free list */
      if((p = morecore(nunits)) == NULL)
	return NULL;                                    /* none left */
  }
}

/* realloc
 *
 * realloc returns a pointer to the reallocated area.
 * It relies on malloc and free defined above to execute.
 *
 * @param    void *ptr
 * @param    size_t size
 */
void * realloc(void *ptr, size_t size){

  if(ptr == NULL && size > 0){
    ptr = malloc(size);
    return ptr;
  }

  if(ptr != NULL && size == 0){
    free(ptr);
    return NULL;
  }

  if(ptr == NULL && size <= 0){
    return NULL;
  }

  Header * headerPointer =  (Header *)ptr-1; /* För att få tillgång till header. */
  size_t old_size = (headerPointer->s.size-1)*sizeof(Header);
  Header * newAreaPointer = malloc(size); /* Malloc tar size i bytes */
  Header * result;
  
  if(old_size <= size){
    result = memcpy(newAreaPointer,ptr,old_size);
  }
  if(old_size > size){
    result = memcpy(newAreaPointer,ptr,size);
  }

  free(ptr);

  return newAreaPointer;
}
