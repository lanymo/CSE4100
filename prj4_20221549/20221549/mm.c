/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * This version avoids using global variables for heap management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20221549",
    /* Your full name*/
    "HyoLim Kim",
    /* Your email address */
    "jeonghh04@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 /*word & header/footer size (bytes)*/
#define DSIZE 8 /*Double word size (bytes)*/
#define CHUNKSIZE (1<<12) /*Extend heap by this amount (bytes)*/

#define MAX(x,y) ((x) > (y) ? (x) : (y))

/*Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc))

/*Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*Read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) -WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char*)(bp) - DSIZE))
#define PRED_P(bp) (*(void**)(bp))
#define SUCC_P(bp) (*(void**)((bp) + WSIZE))

/* Static functions to handle the heap without global variables */
static void* extend_heap( size_t asize);
static void* coalesce(void *bp);
static void* find_fit( size_t asize);
static void place(void *bp, size_t asize);


char *heap_listp; /* Pointer to the first block */
//char *free_listp; /* Pointer to the first free block */

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1) { return -1; }

    PUT(heap_listp, 0); /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); /* prologue header */
    PUT(heap_listp + (2 * WSIZE), heap_listp + (3 * WSIZE));
    PUT(heap_listp + (3 * WSIZE), heap_listp + (2 * WSIZE));
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1)); /* epilogue header */

    heap_listp += (2*WSIZE);

    if (extend_heap( CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}


void insert_Block(void *bp){
    SUCC_P(bp) = SUCC_P(heap_listp);
    PRED_P(bp) = heap_listp;
    PRED_P(SUCC_P(heap_listp)) = bp;
    SUCC_P(heap_listp) = bp;
}

void remove_Block(void *bp){
    /*
    if (bp == free_listp){
        PRED_P(SUCC_P(bp)) = NULL;
        free_listp = SUCC_P(bp);
    }
    */
    //else{
        SUCC_P(PRED_P(bp)) = SUCC_P(bp);
        PRED_P(SUCC_P(bp)) = PRED_P(bp);
    
    //}
}


static void* coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc){ /* Only next block is free */
        remove_Block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else if(!prev_alloc && next_alloc){ /* Only previous block is free */
        remove_Block( PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else if(!prev_alloc && !next_alloc){ /* Both blocks are free */
        remove_Block(PREV_BLKP(bp));
        remove_Block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    insert_Block(bp);

    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore spurious requests */
    if (size == 0){
        return NULL;
    }

    asize = ALIGN(size + SIZE_T_SIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL){
        place( bp, asize);
        return bp;
    }

    /* No fit found. Need to get more memory */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) {return NULL;}
    place(bp, asize);
    return bp;
}


static void *find_fit(size_t asize){
    void *bp;
    

    for(bp = SUCC_P(heap_listp); GET_ALLOC(HDRP(bp)) == 0; bp = SUCC_P(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }

    return NULL; /* No fit found */
}

static void place(void *bp, size_t asize){
    size_t cur_size = GET_SIZE(HDRP(bp));

    remove_Block(bp);

    if((cur_size - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(cur_size-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(cur_size-asize,0));
        insert_Block(NEXT_BLKP(bp));
    }else{
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1));
    }
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1){return NULL;}

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce( bp);
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if (bp == NULL) return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce( bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;  
    void *newptr;        
    size_t copySize;     
    
    if (oldptr == NULL) return mm_malloc(size);
    if (size == 0){
        mm_free(oldptr);
        return NULL;
    }

    size_t new_size = ALIGN(size + SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    size_t N_Size = GET_SIZE(HDRP(NEXT_BLKP(oldptr))) + copySize;

    if(new_size <= copySize) return oldptr;
    else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && new_size <= N_Size){
        remove_Block(NEXT_BLKP(oldptr));
        PUT(FTRP(oldptr), PACK(0,0));
        PUT(HDRP(oldptr), PACK(N_Size, 1));
        PUT(FTRP(oldptr), PACK(N_Size, 1));
        return oldptr;
    }
    else{
        newptr = mm_malloc(new_size);
        if(newptr == NULL) {return NULL;}
        place(newptr, new_size);
        memcpy(newptr, oldptr, new_size); 
        mm_free(oldptr);
        return newptr;
    }
}