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


// ------------------------------------------- Review 잘 부탁드립니다 --------------------------------------------------- //
// Explicit 리스트, first-fit으로 구현하였고 분할 부분을 개선하여 86점으로 통과했습니다!

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



// 메모리가 부족할 때, heap을 늘려주는 함수
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

    // 물리적인 주변 block과 합쳐야 할지 알기 위해 coalesce 호출!!
    return coalesce(bp);
}




/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // empty heap을 만들기
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (2 * WSIZE), NULL);
    PUT(heap_listp + (3 * WSIZE), NULL);
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE * 2, 1));
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));

    // heap_listp += (2 * WSIZE);  explict 리스트의 head로 이용하기 위한 free_listp 정의
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
    size_t asize;
    size_t extendsize;
    char *bp;

    // size 0인 경우 제외
    if (size == 0)
        return NULL;
    
    // size를 조정해주기! header, footer를 위한 8바이트, 그리고 더블 워드 정렬을 위해 ASIGN한다.
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size+DSIZE);
    
    // find_fit 해서 적절한 곳에 메모리 할당
    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize);
        return bp;
    }

    // fit을 찾지 못했다면 heap 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    bp = place(bp, asize);
    return bp;
}


// first-fit을 이용했습니다.
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


// 최소 요구 크기를 기준으로, 2가지 case로 나누어 분할을 진행합니다.
static void *place(void *bp, size_t asize)
{
    size_t osize = GET_SIZE(HDRP(bp));

    remove_block(bp);
    // 할당 후 잔여 블록이 최소 블록 사이즈를 충족하지 못하는 경우, 분할하지 않고 전체 블록을 할당한다. 
    // (할당 블록의 경우, PRED / SUCC이 필요하지 않으므로 최소 블록의 크기는 4 WSIZE이다.)
    if ((osize - asize) <= 4 * WSIZE)
    {
        PUT(HDRP(bp), PACK(osize, 1));
        PUT(FTRP(bp), PACK(osize, 1));
    }
    // 최소 블록 사이즈를 충족할 경우, 2가지로 나누었다.
    // binary.rep의 효율을 상승시키기 위해 96byte 이상의 크기에 대해서는 할당 블록을 뒤에 배치하고, 비할당 블록을 앞에 위치했다.
    // -> 이 효과로 96byte 이상의 블록이 free 된 후, 기존보다 더 큰 블록이 들어 올 때, 외부 단편화를 최소화할 수 있다.
    else if (asize >= 96)
    {
        PUT(HDRP(bp), PACK(osize - asize, 0));
        PUT(FTRP(bp), PACK(osize - asize, 0));
        put_free_block(bp);
        
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

    }
    // 96 이상의 블록이 아닌 경우에는 기존처럼 할당 블록을 앞에, 비할당 블록을 뒤에 배치하였다.
    else
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        PUT(HDRP(NEXT_BLKP(bp)), PACK(osize - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(osize - asize, 0));

        put_free_block(NEXT_BLKP(bp));
    }
    return bp;
}

// 새로운 free 블럭이 생겼을 때, LIFO 방식을 위해 첫 헤드에 넣어줍니다.
void put_free_block(void *bp)
{
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(bp) = NULL;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}


// free 블럭에 메모리를 할당 했을 때, 삭제된 노드를 연결리스트에서 제거하고 연결을 이어줍니다.
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

// 4가지 경우로 나누어 연결을 진행합니다.
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