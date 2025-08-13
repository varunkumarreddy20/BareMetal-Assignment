#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define POOL_SIZE (102400)   // 100 KB pool(100*1024)
#define MAX_BLOCKS 1024      // Max Blocks choosen 1024 not to waste more ram usage on metadata
//#define RUN_TEST3          // use this conditional compilation for running automated test case 3
typedef struct {
    int     allocated;   // It Displays Allocation status,0 for free and 1 for allocated
    int     alloc_id;    // It Displays ID for Blocks (0 if free)
    size_t  offset;      // To know the location of the Blocks(0 .. POOL_SIZE-1)
    size_t  size;        // Number of bytes to store in this Block
} BlockDesc;

/* Static raw memory pool */
static unsigned char memory_pool[POOL_SIZE];

/* Block Descripters to store metadata outside the memory pool */
static BlockDesc blocks[MAX_BLOCKS];
static int block_count = 0;
static int next_alloc_id = 1;

/* To initialize an entire pool */
static void init_pool(void) 
{
    if (block_count != 0) return;
    blocks[0].allocated = 0;
    blocks[0].alloc_id = 0;
    blocks[0].offset = 0;
    blocks[0].size = POOL_SIZE;
    block_count = 1;
}

/* To create an empty space and move all the existing descripters to right so that a new descripter with metadata
    in that empty spcae for each insertion (to be useful at the time of allocation)*/
static int shift_right_and_insert_space(int i) 
{
    if (block_count + 1 >= MAX_BLOCKS) return 0; // Max Blocks already inserted can't insert
    for (int j = block_count; j > i; --j) 
    {
        blocks[j] = blocks[j - 1];
    }
    block_count++;
    return 1;
}

/* It is used to merge two spaces into and store add the metadata */
static void remove_block_at(int i) 
{
    for (int j = i; j < block_count - 1; ++j) 
    {
        blocks[j] = blocks[j + 1];
    }
    block_count--;
}

/* It is used to find best-fit free block index for requested size*/
static int find_best_fit_index(size_t want)
{
    int best = -1;
    size_t best_size = (size_t)-1;
    for (int i = 0; i < block_count; ++i) 
    {
        if (!blocks[i].allocated && blocks[i].size >= want) 
        {
            if (best == -1 || blocks[i].size < best_size) 
            {
                best = i;
                best_size = blocks[i].size;
            }
        }
    }
    return best;
}

/* It is used to merge two or more than two free blocks that are physically adjacent in the memory into a single large free block
    By using offsets we can know the location of the block */
static void coalesce_free_blocks(void) 
{
    if (block_count <= 1) return;
    for (int i = 0; i < block_count - 1; ++i) 
    {
        /* If current and next are both free and physically adjacent, merge them */
        if (!blocks[i].allocated && !blocks[i+1].allocated) 
        {
            if (blocks[i].offset + blocks[i].size == blocks[i+1].offset) 
            {
                blocks[i].size += blocks[i+1].size;
                remove_block_at(i+1);
                --i; 
                if (i < -1) i = -1; 
            }
        }
    }
}

/* It is used to find the head of the block ,If the provided pointer is not the start of block or incorrect pointer it returns  */
static int find_block_index_by_ptr(int *ptr) 
{
    if (!ptr) return -1;
    unsigned char *p = (unsigned char*)ptr;
    unsigned char *base = memory_pool;
    if (p < base || p >= base + POOL_SIZE) return -1;
    size_t off = (size_t)(p - base);
    for (int i = 0; i < block_count; ++i) {
        if (blocks[i].offset == off) return i; /* must point to start of payload */
    }
    return -1;
}

/* It is used to returns pointer to user data as int* (or NULL on failure) */
int *allocate(int size) {
    if (size <= 0) return NULL;
    if ((size_t)size > POOL_SIZE) return NULL;

    /* to find smallest free block >= requested size */
    int best = find_best_fit_index((size_t)size);
    if (best == -1) 
    {
        coalesce_free_blocks();
        best = find_best_fit_index((size_t)size);
        if (best == -1) return NULL; /* still no space */
    }

    /* if exact fit, mark allocated and return pointer */
    if (blocks[best].size == (size_t)size) {
        blocks[best].allocated = 1;
        blocks[best].alloc_id = next_alloc_id++;
        return (int*)(memory_pool + blocks[best].offset);
    }
    size_t original_offset = blocks[best].offset;
    size_t original_size = blocks[best].size;
    /*To prevent allocation failure because metadata is full , avoids crash or null return */
    if (!shift_right_and_insert_space(best + 1)) 
    {
        /* cannot create new descriptor -> try to allocate whole block */
        blocks[best].allocated = 1;
        blocks[best].alloc_id = next_alloc_id++;
        return (int*)(memory_pool + blocks[best].offset);
    }

    /* adjust descriptors */
    blocks[best].allocated = 1;
    blocks[best].alloc_id = next_alloc_id++;
    blocks[best].size = (size_t)size;
    blocks[best].offset = original_offset;

    /* new remainder block occupies remaining bytes */
    blocks[best + 1].allocated = 0;
    blocks[best + 1].alloc_id = 0;
    blocks[best + 1].offset = original_offset + (size_t)size;
    blocks[best + 1].size = original_size - (size_t)size;

    return (int*)(memory_pool + blocks[best].offset);
}

/* To deallocate a pointer (int*)in the pool. If ptr invalid it should print error */
void deallocate(int *ptr) 
{
    if (!ptr)
    {
        printf("Invalid entry of pointer address \n");
        return;
    }
    
    int idx = find_block_index_by_ptr(ptr);
    if (idx == -1) 
    {
        /* invalid pointer (not pointing to start of an allocated block) */
        fprintf(stderr, "deallocate: pointer %p not recognized as block start\n", (void*)ptr);
        return;
    }

    if (!blocks[idx].allocated) 
    {
        fprintf(stderr, "deallocate: block at %p is already free\n", (void*)ptr);
        return;
    }
    printf(" pointer address %p is De-allocated\n ",ptr);
    /* mark free */
    blocks[idx].allocated = 0;
    blocks[idx].alloc_id = 0;

    /* coalesce adjacent free blocks */
    coalesce_free_blocks();
}

/*  print pool state  */
void print_pool(void) {
    
    puts("\n=== Pool (physical order) ===");
    for (int i = 0; i < block_count; ++i) {
        if (blocks[i].allocated) {
            printf("Block %d - Size: %zu - Status: Allocated (ID %d)\n",
                   i + 1, blocks[i].size, blocks[i].alloc_id);
        } else {
            printf("Block %d - Size: %zu - Status: Free\n",
                   i + 1, blocks[i].size);
        }
    }

    puts("\n=== Free List (logical) ===");
    int free_i = 1;
    for (int i = 0; i < block_count; ++i) {
        if (!blocks[i].allocated) {
            printf("Free %d - Size: %zu\n", free_i++, blocks[i].size);
        }
    }
    puts("============================\n");
}

int main()
{
    printf("Bare-metal style memory allocator (100 KB pool)\n");
    init_pool();
 
    /* ===============================
            Automated test cases
       ===============================*/
    printf("\n--- Running Automated Test Cases ---\n");

    // 1. Minimum allocation
    int *t1 = allocate(1);
    printf("Test1 (1 byte): %s\n", t1 ? "PASS" : "FAIL");
    deallocate(t1);

    // 2. Maximum allocation
    int *t2 = allocate(POOL_SIZE);
    printf("Test2 (100 KB): %s\n", t2 ? "PASS" : "FAIL");
    deallocate(t2);

    // 3. Coalescing check
    int *a = allocate(128);
    int *b = allocate(256);
    deallocate(a);
    deallocate(b); 
    printf("Test 3\n ");
    print_pool();

    // 4. Best-fit check
    int *p1 = allocate(1024);
    int *p2 = allocate(2048);
    int *p3 = allocate(4096);
    deallocate(p2);
    int *p4 = allocate(512); 
    printf("Test 4\n ");
    print_pool();
    deallocate(p1);
    deallocate(p3);
    deallocate(p4);

    // 5. Invalid pointer free
    int x;
    printf("Test 5\n");
    deallocate(&x);

    // 6. Double free detection
    int *p = allocate(1024);
    printf("Test 6\n");
    deallocate(p);
    deallocate(p);
    
    // 7. Allocation until pool is full//
    #ifdef RUN_TEST3 
    int *ptrs[200];
    int i, count = 0;
    while ((ptrs[count] = allocate(512)) != NULL) count++;
    printf("Test7 (fill pool with 512-byte blocks): Allocated %d blocks\n", count);
    for (i = 0; i < count; i++) deallocate(ptrs[i]);
    #endif

    printf("\n--- Automated Tests Complete ---\n");

    while (1) 
    {
        printf("\n1. Allocate\n2. Deallocate\n3. Print Pool\n4. Exit\nChoice: ");
        int ch = 0;
        if (scanf("%d", &ch) != 1) 
        {
            int c; while ((c = getchar()) != '\n' && c != EOF);
            printf("Invalid input.\n");
            continue;
        }
        if (ch == 1) 
        {
            int size;
            printf("Enter size in bytes: ");
            if (scanf("%d", &size) != 1) 
            { printf("Invalid\n"); continue; }
            int *p = allocate(size);
            if (p) 
            {
                printf("Allocated %d bytes -> %p\n", size, (void*)p);
            } else 
            {
                printf("Allocation failed.\n");
            }
        } 
        else if (ch == 2) 
        {
            char buf[64];
            printf("Enter pointer to deallocate (copy the pointer printed earlier),or type 'id N' to free by ID:\n");
            printf("check the id that needed to be De-allocated correctly because some of the ID's are used for Automated test cases (use print function for exact Id numbers)\n");
            /* We accept two forms:
               - "0xabc..." pointer form -> parse as hex
               - "id N" -> free block with alloc_id == N
            */
            int c = getchar(); // consume newline
            if (!fgets(buf, sizeof(buf), stdin)) { continue; }
            if (strncmp(buf, "id", 2) == 0) 
            {
                int id = 0;
                if (sscanf(buf + 2, "%d", &id) == 1 && id > 0) 
                {
                    /* find block by id and free */
                    int found = 0;
                    for (int i = 0; i < block_count; ++i) {
                        if (blocks[i].allocated && blocks[i].alloc_id == id)
                        {
                            deallocate((int*)(memory_pool + blocks[i].offset));
                            found = 1;
                            break;
                        }
                    }
                    if (!found) printf("ID P%d not found.\n", id);
                }
                else 
                {
                    printf("Invalid id format.\n");
                }
            } 
            else 
            {
                /* try parse pointer (hex) */
                void *pv = NULL;
                if (sscanf(buf, "%p", &pv) == 1 && pv != NULL) 
                {
                    deallocate((int*)pv);
                } else 
                {
                    printf("Invalid pointer format.\n");
                }
            }
        } 
        else if (ch == 3) 
        {
            print_pool();
        } 
        else if (ch == 4) 
        {
            break;
        } 
        else 
        {
            printf("Invalid choice.\n");
        }
    }

    return 0;
}
