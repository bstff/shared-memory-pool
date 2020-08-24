/*
 * 管理共享内存的分配与释放
 */
#include "mm.h"
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>

/* $begin mallocmacros */
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size_t)(size) + (ALIGNMENT-1)) & ~(ALIGNMENT - 1))

/* Returns true if p is ALIGNMENT-byte aligned */
#define IS_ALIGNED(p)  ((((size_t)(p)) & (ALIGNMENT - 1)) == 0)

/*
 * If NEXT_FIT defined use next fit search, else use first-fit search
 */
//#define NEXT_FITx

/* Basic constants and macros */
#define SIZE_T_SIZE (sizeof(size_t))

#define PTR_SIZE (sizeof(char *))

#define MIN_BLOCK_SIZE (ALIGN( (SIZE_T_SIZE << 1) + SIZE_T_SIZE + (PTR_SIZE << 1) ))


//#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))
#define GET_PTR(p)       (*(char **)(p))
#define PUT_PTR(p, val)  (*(char **)(p) = (char*)(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~(ALIGNMENT-1))                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - SIZE_T_SIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - 2 * SIZE_T_SIZE) //line:vm:mm:ftrp
#define SUCCRP(bp)       ((char **)(bp) + 1)                      //line:vm:mm:hdrp
#define PREDRP(bp)       ((char **)(bp))                      //line:vm:mm:hdrp
//#define FIRSTER(heap_listp)       ((char **)(heap_listp))
//#define LASTER(heap_listp)       ((char **)(heap_listp) + 1)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - SIZE_T_SIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - 2 * SIZE_T_SIZE))) //line:vm:mm:prevblkp
#define NEXT_FBLKP(bp) (*SUCCRP(bp))
#define PREV_FBLKP(bp) (*PREDRP(bp))

/* $end mallocmacros */



// #define MAX_HEAP (512*(1<<20))  /* 20 MB */
#define MAX_HEAP (512)  /* 20 MB */
/* Hard-coded keys for IPC objects */
#define OBJ_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)


/* Global variables */
static void *key_addr = (void *)0x400d0000;
static void * heap_listp = 0;  /* Pointer to first block */

/* Function prototypes for internal helper routines */
static void * extend_heap(size_t words);
static void *place(void *bp, size_t size);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static inline void rm_fblock(void *bp);
static void insert_fblock (void *bp);

static void *mem_sbrk(int incr);
static int is_allocated(void *ptr);

static int shmid = -1;
static void *shmp;
//static int mutex = SemUtil::get(IPC_PRIVATE, 1);

static void *mem_start_brk;  /* points to first byte of heap */
static void *mem_brk;        /* points to last byte of heap */
static void *mem_max_addr;   /* largest legal heap address */
static size_t mem_max_size;



/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
void *mm_malloc(size_t size)
{
  size_t newsize;      /* Adjusted block size */
  void *ptr, *aptr;

  /* Ignore spurious requests */
  if (size == 0)
    return NULL;

  newsize = ALIGN(size +  (SIZE_T_SIZE << 1) +  (PTR_SIZE << 1) );

    //fprintf(stderr, "mm_malloc : size=%u\n", size);
  /* Search the free list for a fit */
  if ((ptr = find_fit(newsize)) != NULL)
  {
    aptr = place(ptr, newsize);
    return aptr;
  } else {
    return NULL;
  }

}

/*
 * mm_free - Free a block
 */
void mm_free(void *ptr)
{
  if (ptr == 0)
    return;

  /*
   *if (!is_allocated(ptr) ) {
   *  printf("Free error: %p is not a allocated block\n", ptr);
   *  return;
   *}
   */

  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
}



/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if (size == 0)
  {
    mm_free(ptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if (ptr == NULL)
  {
    return mm_malloc(size);
  }

  /*if (!is_allocated(ptr) ) {*/
  /*printf("realloc error: %p is not a allocated block\n", ptr);*/
  /*return 0;*/
  /*}*/

  newptr = mm_malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if (!newptr)
  {
    return 0;
  }

  /* Copy the old data. */
  oldsize = GET_SIZE(HDRP(ptr));
  if (size < oldsize) oldsize = size;
  memcpy(newptr, ptr, oldsize);

  /* Free the old block. */
  mm_free(ptr);

  return newptr;
}
/*
 * The remaining routines are internal helper routines
 */

void *mem_sbrk(int incr)
{
  void *old_brk = mem_brk;

  if ( (incr < 0) || (((char *)mem_brk + incr) > (char*)mem_max_addr))
  {
    // errno = ENOMEM;
    // fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
    return (void *) - 1;
  }
  mem_brk =(void*)((char *)mem_brk + incr);
  return (void *)old_brk;
}

// void *get_mm_start_brk() {
//   return mem_start_brk;
// }

// size_t get_mm_max_size() {
//   return mem_max_size;
// }

/*
 * mm_init - Initialize the memory manager, M unit
 */
bool mm_init(const int key, size_t heap_size)
{
  
  //同一进程内已经初始化过了
  if (shmid != -1){
    return false;
  }

  bool first = true;
  int offset = 0;
  if(heap_size <= 0) {
    heap_size = MAX_HEAP;
  }
  heap_size = (heap_size*(1<<20));

  shmid = shmget(key, heap_size, IPC_CREAT | IPC_EXCL | OBJ_PERMS);
  if (shmid == -1) {
    first = false;
    shmid  = shmget(key, 0, 0);
  }
  if (shmid == -1)
      return false;
  shmp = shmat(shmid, key_addr, 0);
  if ((long)shmp == -1)
      return false;

  // 闪过hashtable尺寸,并对齐
  mem_start_brk = (void*)ALIGN((size_t)((char *)shmp + offset + ALIGNMENT));
  // 计算最大可用尺寸
  mem_max_addr = (void *)((char *)shmp+ heap_size);
  mem_max_size = (char *)mem_max_addr - (char*)mem_start_brk;
  // 赋值给初始位置
  mem_brk = mem_start_brk;
  void *free_listp;
  /* Create the initial empty heap */
  // 闪过3个SIZE_t和2个ptr尺寸
  int initsize = ALIGN(3 * SIZE_T_SIZE + 2 * PTR_SIZE);
  // heap 链表在start加上闪过对齐后的一个SIZE_t的位置
  heap_listp = (char *)mem_start_brk + initsize - 2 * SIZE_T_SIZE - 2 * PTR_SIZE;

  if(!first) {
    //其他进程已经创建过了共享内存
    return first;
  }

 
  /*
          header                      foot    next->header
    0       8      16        24        32        40        48
    +------------------------------------------------------+
    |       |   41  |                   |   41   |    1    |
    | align |(32, 1)|                   |(32, 1) |pack(0,1)|
    |       |  size |                   |  size  |         |
    +------------------------------------------------------+
mem_start_brk -- heap_listp->header -- heap_listp start -- foot -- packed(no use?) -- initsize->end
  */

  // 指向已经使用的的内存的最后一个字节
  if ((mem_sbrk(initsize)) == (void *) - 1)
    return false;
  
  PUT((char *)mem_start_brk + initsize - SIZE_T_SIZE, PACK(0, 1));   /* Epilogue header */
  /*PUT(HDRP(heap_listp), PACK(initsize - SIZE_T_SIZE, 1));
  PUT(FTRP(heap_listp), PACK(initsize - SIZE_T_SIZE, 1));*/

  PUT(HDRP(heap_listp), PACK(initsize - SIZE_T_SIZE, 1));
  PUT(FTRP(heap_listp), PACK(initsize - SIZE_T_SIZE, 1));
  
  /**
   * here the heap_listp can be look as a ancher which concat the header and tail of free-list to form a ring, and the heap_list itself will never be used as a free block
  */
  // 做一个环形列表
  PUT_PTR(SUCCRP(heap_listp), heap_listp);
  PUT_PTR(PREDRP(heap_listp), heap_listp);
  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if ((free_listp = extend_heap(mem_max_size - initsize - ALIGNMENT)) == NULL)
    return false;

  return first;
}

bool mm_destroy(void) {
  struct shmid_ds shmid_ds;
  //detache
  if (shmdt(shmp) == -1) {
    return false;
  }

  
  if(shmctl(shmid, IPC_STAT, &shmid_ds) == 0) {
    //LoggerFactory::getLogger().debug("shm_nattch=%d\n", shmid_ds.shm_nattch);
    if(shmid_ds.shm_nattch == 0) {
      //remove shared memery
       if (shmctl(shmid, IPC_RMID, 0) == -1)
          return false;

       return true;

    } 
    return false;
  } else {
    return false;
  }
 
  return false;
}
/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */

  /*
  接上图
    next->header  start              cur->foot  next->header
    40       48                                736-8     (736) end
    +------------------------------------------------------+
    |  736   |                          |  736   |    1    |
    |(736,0) |                          |(736,0) |pack(0,1)|
    |  size  |                          |  size  |         |
    +------------------------------------------------------+
initsize(->end) -- bp start -- heap_listp -- foot -- packed(no use?)
  */

static void *extend_heap(size_t size)
{
  void *bp;
  size = ALIGN(size);

  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;                                        //line:vm:mm:endextend

  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
  PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

  /* Coalesce if the previous block was free */
  return coalesce(bp);                                          //line:vm:mm:returnblock
}

static void insert_fblock (void *bp)
{
  //后进先出的方式插入，即插入链表头位置

  // insert into the header of the free list
  PUT_PTR(SUCCRP(bp), NEXT_FBLKP(heap_listp)); //the successor of bp point to the old first free block
  PUT_PTR(PREDRP(NEXT_FBLKP(heap_listp)), bp); //the predecessor of the old first free block point to bp
  PUT_PTR(SUCCRP(heap_listp), bp); // successor of the ancher(锚点) point to bp
  PUT_PTR(PREDRP(bp), heap_listp); //the predecessor of bp point to heap_listp

}
/**
 * remove a block form free list
 */
static inline void rm_fblock(void *rbp)
{
  // the successor of the previous block of rbp point to next block of rbp

  PUT_PTR(SUCCRP(PREV_FBLKP(rbp)), NEXT_FBLKP(rbp));
  // the predecessor of then next block of rbp point to previous block of rbp
  PUT_PTR(PREDRP(NEXT_FBLKP(rbp)), PREV_FBLKP(rbp));
}
/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */

/*
            start                             next->header end
    +------------------------------------------------------+
    |  736   |                          |  736   |    1    |
    |(736,0) |                          |(736,0) |pack(0,1)|
    |  size  |                          |  size  |         |
    +------------------------------------------------------+
cur->header                         cur->foot
*/
// 连接free内存
static void *coalesce(void *bp)
{
  // 获取下一个块, 移动到end
  void *nbp = NEXT_BLKP(bp);
  // 获取上一个块, 从start反向移动两个SIZE_T_SIZE正好到上一个块的foot
  void *pbp = PREV_BLKP(bp);
  // 获取上一个块的大小
  size_t prev_alloc = GET_ALLOC(FTRP(pbp));
  // 获取下一个块大小
  size_t next_alloc = GET_ALLOC(HDRP(nbp));
  // 获取本块大小
  size_t size = GET_SIZE(HDRP(bp));

  // 大小都以16对齐
  // 都在使用中,插入free list
  if (prev_alloc && next_alloc)              /* Case 1 */
  {
    insert_fblock(bp);
    return bp;
  }
  // 前一块正在使用,下一块没用,融合下一块,并删除
  else if (prev_alloc && !next_alloc)        /* Case 2 */
  {
    size += GET_SIZE(HDRP(nbp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    rm_fblock(nbp);
    insert_fblock(bp);
  }

  else if (!prev_alloc && next_alloc)        /* Case 3 */
  {
    size += GET_SIZE(HDRP(pbp));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(pbp), PACK(size, 0));
    bp = pbp;
  }

  else                                       /* Case 4 */
  {
    nbp = nbp;
    size += GET_SIZE(HDRP(pbp)) + GET_SIZE(FTRP(nbp));
    PUT(HDRP(pbp), PACK(size, 0));
    PUT(FTRP(nbp), PACK(size, 0));
    bp = pbp;
    rm_fblock(nbp);
  }

  return bp;
}

/*
 * place - Place block of size bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void * place(void *bp, size_t size)
{
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - size) >= MIN_BLOCK_SIZE)
  {
    //free list keep no change
    PUT(HDRP(bp), PACK(csize - size, 0));
    PUT(FTRP(bp), PACK(csize - size, 0));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
  }
  else
  {
    // mark allocateed
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
    rm_fblock(bp);
  }
  return bp;
}

static int is_allocated(void *ptr)
{
  if (ptr == NULL)
    return 0;
  int is_alloced = 0;
  void *bp;
  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if ((ptr == bp) && GET_ALLOC(HDRP(bp)) )
    {
      is_alloced = 1;
      break;
    }
  }
  if (!is_alloced)
  {
    return 0;
  }
  return 1;

}

/*
 * find_fit - Find a fit for a block with size bytes
 */
static void *find_fit(size_t size)
{
  void *bp;

  for (bp = NEXT_FBLKP(heap_listp); bp != heap_listp; bp = NEXT_FBLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp))))
    {
      return bp;
    }
  }
  return NULL; /* No fit */
}

// =============================check heap========================
static void printblock(void *bp)
{
  size_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

  if (hsize == 0)
  {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%lx:%c] footer: [%lx:%c]\n",
         bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

static int checkblock(void *bp)
{
  if (!IS_ALIGNED(bp))
  {
    printf("Error: %p is not doubleword aligned\n", bp);
    return 0;
  }
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
  {
    printf("Error: header does not match footer\n");
    return 0;
  }
  return 1;
}

static int checkblocklist(int verbose)
{
  int valid = 1;
  void *bp = heap_listp;

  if (verbose > 1)
    printf("Heap (%p):\n", heap_listp);

  /*if ((GET_SIZE(HDRP(heap_listp)) != 2 * SIZE_T_SIZE) || !GET_ALLOC(HDRP(heap_listp)))*/
  /*printf("Bad prologue header\n");*/

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (verbose > 1)
      printblock(bp);

    if (!checkblock(bp))
    {
      valid = 0;
    }
  }

  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
  {
    printf("Bad epilogue header\n");
    valid = 0;
  }
  if (verbose > 1)
    printblock(bp);
  return valid;
}

void printfreeblock(void *bp)
{

}

void check_freelist()
{
  void *bp;

  for (bp = NEXT_FBLKP(heap_listp); bp != heap_listp; bp = NEXT_FBLKP(bp))
  {
    printfreeblock(bp);
  }

}
/*
 * mm_check - Minimal check of the heap for consistency
 */
int mm_checkheap(int verbose)
{
  return checkblocklist(verbose);
}
