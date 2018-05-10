#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ext2.h"

// my addition to see if it is file or folder
#include <sys/stat.h>

#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
#define IMAGE "image_indirect.img"

typedef unsigned char bmap;
#define __NBITS (8 * (int) sizeof (bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap) 1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT (d)] |= __BMMASK (d)))
#define BM_CLR(d, set) ((set[__BMELT (d)] &= ~__BMMASK (d)))
#define BM_ISSET(d, set) ((set[__BMELT (d)] & __BMMASK (d)) != 0)

unsigned int block_size = 0;
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

int fd;

struct _Vector{
    int* _list;
    int _size;
} typedef Vector;

void bitmap_block_reader(bmap* block_bitmap, struct ext2_super_block super)
{
    for (int i = 1; i < super.s_blocks_count; i++){
        int j = i;
        while (j > 9)
            j -= 10;
        //printf("%d", j);
    }
    // printf("\n");
    for (int i = 0; i < super.s_blocks_count; i++){
        if (BM_ISSET(i,block_bitmap)){
            //printf("+");    // in use
        }
        else{
            //printf("-");    // empty
        }
    }
    //printf("\n");
    return;
}


void printDirectBlocks(struct ext2_inode inode, int index)
{
    int* directBlocks = malloc(sizeof(int)*12);
    for(int i=0; i < 12; i++)
        directBlocks[i] = -1;

    int count = 0;
    for(int i=0; i < 12; i++){
        if (!(inode.i_block[i]))
            continue;
        else if (i < 12 )         // direct blocks
            count++;
            directBlocks[i] = inode.i_block[i];
    }

    printf("DIRECT:\n");
    printf("    #: %d\n", count);
    printf("    [");
    for(int i=0; i < 12; i++)
        if (directBlocks[i] != -1)
            printf("%d ", directBlocks[i]);
    printf("]\n");
    // free(directBlocks);
}

Vector readBlockIntoArray(int blockNo, struct ext2_super_block super, int fd)
{
    int num_of_ptrs = block_size/4;
    Vector result;
    if (blockNo)
    {
        lseek(fd, BLOCK_OFFSET(super.s_first_data_block)+ block_size* (blockNo -1), SEEK_SET);
        int arr = 0;
        int count = 0;
        for(int i=0; i<num_of_ptrs; i++)
        {
            read(fd, &arr, 4);
            if ( arr != 0)
                count++;   
        }

        // allocate an array for indirect blocks
        int* singleIndirectBlocks = malloc(sizeof(int)*count);
        for(int i=0; i < count; i++)
            singleIndirectBlocks[i] = -1;

        // move cursor back to its position, then read into array
        lseek(fd, BLOCK_OFFSET(super.s_first_data_block)+ block_size* (blockNo -1), SEEK_SET);
        for(int i=0; i<num_of_ptrs; i++)
        {
            read(fd, &arr, 4);
            if ( arr != 0)
                singleIndirectBlocks[i] = arr; 
        }

        Vector result;
        result._list = singleIndirectBlocks;
        result._size = count;
        return result;
    }
    else
    {
        result._list = NULL;
        result._size = 0;
        return result;
    }
}

void printSingleIndirectBlocks(int blockNo, int index, struct ext2_super_block super, int fd)
{
    
    int num_of_ptrs = block_size/4;
   
    // inode block 12 holds which inode holds directs
    if( blockNo )
    {
        printf("SINGLE:\n");
        printf("    SingleHolderBlock: %d\n", blockNo);

        Vector singleIndirectBlocks = readBlockIntoArray(blockNo, super, fd);
        int* array = singleIndirectBlocks._list;
        int count = singleIndirectBlocks._size;

        // print to screen
        printf("    #: %d\n", count);
        printf("    [");
        for(int i=0; i < count; i++)
            printf("%d ", array[i]);
        printf("]\n");

        // free(array);
    }
}

void printDoubleIndirectBlocks(int blockNo, int index, struct ext2_super_block super, int fd)
{
    if (blockNo)
    {

        Vector doubleBlock = readBlockIntoArray(blockNo, super, fd);
        int* arrayOfDoubles = doubleBlock._list;
        int countOfDoubles = doubleBlock._size;

        for(int i=0; i<countOfDoubles; i++)
        {
            int blockNoOfSingle = arrayOfDoubles[i];
            printSingleIndirectBlocks(blockNoOfSingle, index, super, fd);
        }
    }
    
}

void printTripleIndirectBlocks(int blockNo, int index, struct ext2_super_block super, int fd)
{
    if (blockNo)
    {

        Vector tripleBlock = readBlockIntoArray(blockNo, super, fd);
        int* arrayOfTriples = tripleBlock._list;
        int countOfTriples = tripleBlock._size;

        for(int i=0; i<countOfTriples; i++)
        {
            int blockNoOfDouble = arrayOfTriples[i];
            printDoubleIndirectBlocks(blockNoOfDouble, index, super, fd);
        }
    }
    
}

void inode_block_reader(struct ext2_inode inode, int index, struct ext2_super_block super, int fd)
{

	printf("Inode No: %d\n", index);

    if (S_ISREG(inode.i_mode))
        printf("FILE, ");
    else if (S_ISDIR(inode.i_mode))
        printf("FOLDER, ");

	printf("Size: %d, #Blocks: %u\n", inode.i_size, inode.i_blocks/(block_size/512));
    if (inode.i_size == 0)
    {
        printf("\n");
        return;
    }

    printDirectBlocks(inode, index);
    printSingleIndirectBlocks(inode.i_block[12], index, super, fd);

    
    if((inode.i_block[13]) )   // double indirect block
            printf("Double   : %u ", inode.i_block[13]);
    if((inode.i_block[14]) )    // triple indirect block
            printf("Triple   : %u ", inode.i_block[14]);
    printf("\n");
}

int main(void)
{
    struct ext2_super_block super;
    struct ext2_group_desc group;
    int fd;

    if ((fd = open(IMAGE, O_RDONLY)) < 0) {
        perror(IMAGE);
        exit(1);
    }

    // read super-block
    lseek(fd, BASE_OFFSET, SEEK_SET);
    read(fd, &super, sizeof(super));
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not a Ext2 filesystem\n");
        exit(1);
    }
    block_size = 1024 << super.s_log_block_size;

    // read group descriptor
    lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
    read(fd, &group, sizeof(group));
    // printf ("Block Bitmap : %d\n", group.bg_block_bitmap);
    // printf ("Inode Bitmap : %d\n", group.bg_inode_bitmap);
    // printf ("Inode Table  : %d\n", group.bg_inode_table);

    printf("Reading from image file " IMAGE ":\n"
           "Blocks count            : %u\n"
           "First non-reserved inode: %u\n",
           super.s_blocks_count,
           super.s_first_ino);

    unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;
    unsigned int inode_table_count = sizeof(struct ext2_inode)*super.s_inodes_count/block_size;
    unsigned int datablocks_start = 1 + group_count + 1 + 1 + inode_table_count;
    printf ("Data Blocks Start @: %u\n", datablocks_start +1);

    // read group descriptor
    lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
    read(fd, &group, sizeof(group));
    printf ("Block Bitmap : %d\n", group.bg_block_bitmap);
    printf ("Inode Bitmap : %d\n", group.bg_inode_bitmap);
    printf ("Inode Table  : %d\n", group.bg_inode_table);
    
    // read block bitmap
    bmap *block_bitmap; block_bitmap = malloc(block_size);
    lseek(fd, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    read(fd, block_bitmap, block_size);
  
    
    bmap* inode_bitmap; inode_bitmap = malloc(block_size);
    lseek(fd, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    read(fd, inode_bitmap, block_size);

    //// read root inode
    struct ext2_inode inode;
    int max = 0;
    for (int i = 0; i < super.s_first_ino; i++){
        if (BM_ISSET(i,inode_bitmap))
        {
                lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+i*sizeof(struct ext2_inode), SEEK_SET);
                read(fd, &inode, sizeof(struct ext2_inode));
                for(int i=0; i < 15; i++){
                    if (inode.i_block[i] > max)
                        max = inode.i_block[i];
                }           
        }
    }
    max += 1;
    printf("START FROM: %d\n",max);


    for (int i = super.s_first_ino; i < super.s_inodes_count; i++){
        if (BM_ISSET(i,inode_bitmap))
        {
                lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+i*sizeof(struct ext2_inode), SEEK_SET);
                read(fd, &inode, sizeof(struct ext2_inode));
                inode_block_reader(inode, i+1, super, fd);              
        }
    }
    printf("#####\n");
    bitmap_block_reader(block_bitmap, super);

    // read root inode
    // struct ext2_inode inode;
    // lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+sizeof(struct ext2_inode), SEEK_SET);
    // read(fd, &inode, sizeof(struct ext2_inode));
    // printf("Reading root inode\n"
    //        "Size     : %u bytes\n"
    //        "Blocks   : %u\n",
    //        inode.i_size,
    //        inode.i_blocks); // in number of sectors. A disk sector is 512 bytes.
    // for(int i=0; i < 15; i++){
    //     if (i < 12)         // direct blocks
    //         printf("Block %2u : %u\n", i, inode.i_block[i]);
    //     else if (i == 12)     // single indirect block
    //         printf("Single   : %u\n", inode.i_block[i]);
    //     else if (i == 13)    // double indirect block
    //         printf("Double   : %u\n", inode.i_block[i]);
    //     else if (i == 14)    // triple indirect block
    //         printf("Triple   : %u\n", inode.i_block[i]);
    // }
    
    close(fd);
    return 0;
}
