/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


// 함수 선언
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void *next_fit(size_t);
static void *place(void *, size_t);

void remove_block(void *bp);
void put_free_block(void *bp);

static char *heap_listp;
static char *free_listp = NULL;




/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "king_wang_zzang",
    /* First member's full name */
    "자헌킹",
    /* First member's email address */
    "자헌이형 이메일",
    /* Second member's full name (leave blank if none) */
    "민우짱",
    /* Second member's email address (leave blank if none) */
    "민우 메일"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 책에 나온 기본 상수 및 매크로
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED_LOC(bp) (HDRP(bp) + WSIZE)
#define SUCC_LOC(bp) (HDRP(bp) + DSIZE)

#define PREC_FREEP(bp) GET(PRED_LOC(bp))
#define SUCC_FREEP(bp) GET(SUCC_LOC(bp))

///////////////////////////////// Block information /////////////////////////////////////////////////////////
/*
 
A   : Allocated? (1: true, 0:false)
RA  : Reallocation tag (1: true, 0:false)
 
 < Allocated Block >
 
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |  | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                                                                                               |
            |                                                                                               |
            .                              Payload and padding                                              .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
 < Free block >
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |RA| A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its predecessor in Segregated list                          |
bp+WSIZE--> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its successor in Segregated list                            |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            .                                                                                               .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
*/
///////////////////////////////// End of Block information /////////////////////////////////////////////////////////




static void *extend_heap(size_t words)
{
    char * bp;
    size_t size;

    // 짝수, 홀수에 따라 size 정하기
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    // 새로 할당하는 메모리의 header, footer 만들기 + epilogue 새로 지정하기
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 이전 block과 합쳐야 할지 알기 위해 coalesce 호출!!
    return coalesce(bp);
}




/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // empty heap을 만들자
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (2 * WSIZE), NULL);
    PUT(heap_listp + (3 * WSIZE), NULL);
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));

    // heap_listp += (2 * WSIZE);
    free_listp = heap_listp + DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
    size_t asize;
    size_t extendsize;
    char *bp;

    // size 0인 경우 제외
    if (size == 0)
        return NULL;
    
    // size를 조정해주기! header, footer를 위한 8바이트, 그리고 기본 2와드 이므로 8바이트
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    // find_fit 해서 적절한 곳에 메모리 심기
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // fit이 없다면
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{
    void *start_bp = free_listp;
    while (!GET_ALLOC(HDRP(start_bp)))
    {
        if (GET_SIZE(HDRP(start_bp)) >= asize)
            return start_bp;
        start_bp = SUCC_FREEP(start_bp);
    }
    return NULL;
}



// long long next_fit_point = 4 * WSIZE;
// static void *next_fit(size_t asize)
// {
//     void *start_bp = mem_heap_lo();
//     printf("%p\n", next_fit_point);
//     while (GET_SIZE(HDRP(start_bp + next_fit_point)) > 0)
//     {
//         if (!GET_ALLOC(HDRP(start_bp + next_fit_point)) && (asize <= GET_SIZE(HDRP(start_bp + next_fit_point))))
//             // start_bp 어딘가에 저장  next_bp
//             return start_bp + next_fit_point;
        
//         next_fit_point += GET_SIZE(HDRP(start_bp + next_fit_point));
//         printf("%p\n", next_fit_point);
//     }

//     next_fit_point = 4 * WSIZE;
//     while (GET_SIZE(HDRP(start_bp + next_fit_point)) > 0)
//     {
//         if (!GET_ALLOC(HDRP(start_bp + next_fit_point)) && (asize <= GET_SIZE(HDRP(start_bp + next_fit_point))))
//             // start_bp 어딘가에 저장  next_bp
//             return start_bp + next_fit_point;
        
//         next_fit_point += GET_SIZE(HDRP(start_bp + next_fit_point));
//     }

//     return NULL;
// }

static void *place(void *bp, size_t asize)
{
    size_t size = GET_SIZE(HDRP(bp));

    remove_block(bp);
    if ((size - asize) >= 6 * WSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(size - asize, 0));
        PUT(FTRP(bp), PACK(size - asize, 0));

        put_free_block(bp);
    }
    
    else
    {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
}

void put_free_block(void *bp)
{
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(bp) = NULL;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}



void remove_block(void *bp)
{
    if (bp == free_listp) {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    else {
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {

    }
        
    else if (prev_alloc && !next_alloc) {
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        remove_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    put_free_block(bp);
    return bp;
}



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}