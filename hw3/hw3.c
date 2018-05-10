#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ext2.h"
#include <vector>

// my addition to see if it is file or folder
#include <sys/stat.h>

#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
#define IMAGE "image_indirect.img"

typedef unsigned char bmap;
#define __NBITS (8 * (int)sizeof(bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap)1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT(d)] |= __BMMASK(d)))
#define BM_CLR(d, set) ((set[__BMELT(d)] &= ~__BMMASK(d)))
#define BM_ISSET(d, set) ((set[__BMELT(d)] & __BMMASK(d)) != 0)

unsigned int block_size = 0;
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

int fd;

void bitmap_block_reader(bmap* block_bitmap, struct ext2_super_block super) {
  for (int i = 1; i < super.s_blocks_count; i++) {
    int j = i;
    while (j > 9) j -= 10;
    printf("%d", j);
  }
  printf("\n");
  for (int i = 0; i < super.s_blocks_count; i++) {
    if (BM_ISSET(i, block_bitmap)) {
      printf("+");  // in use
    } else {
      printf("-");  // empty
    }
  }
  printf("\n");
  return;
}

std::vector<int> printDirectBlocks(struct ext2_inode inode, int index) {
  std::vector<int> directBlocks;

  int count = 0;
  for (int i = 0; i < 12; i++) {
    if (!(inode.i_block[i]))
      continue;
    else  // direct blocks
    {
        count++;
        directBlocks.push_back( inode.i_block[i] );
    }
  }

  printf("DIRECT:");
  printf("    #: %d", count);
  printf("    [");
  for (int i = 0; i < count; i++)
    if (directBlocks[i] != -1) printf("%d ", directBlocks[i]);
  printf("]\n");
  return directBlocks;
}

std::vector<int> readBlockIntoArray(int blockNo, struct ext2_super_block super, int fd) {
  int num_of_ptrs = block_size / 4;
  if (blockNo) {
    lseek(fd,
          BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1),
          SEEK_SET);
    int arr = 0;
    int count = 0;
    for (int i = 0; i < num_of_ptrs; i++) {
      read(fd, &arr, 4);
      if (arr != 0) count++;
    }

    // allocate an array for indirect blocks
    std::vector<int> singleIndirectBlocks;
    singleIndirectBlocks.resize(0);

    // move cursor back to its position, then read into array
    lseek(fd,
          BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1),
          SEEK_SET);
    for (int i = 0; i < num_of_ptrs; i++) {
      read(fd, &arr, 4);
      if (arr == 0)
        break;
      singleIndirectBlocks.push_back(arr);
    }

    return singleIndirectBlocks;
  } else {
    // SCREAM HERE
    return {};
  }
}

std::vector<int> printSingleIndirectBlocks(int blockNo, int index,
                                 struct ext2_super_block super, int fd) {
  int num_of_ptrs = block_size / 4;

  // inode block 12 holds which inode holds directs
  if (blockNo) {
    printf("SINGLE:");
    printf("    SingleHolderBlock: %d", blockNo);

    std::vector<int> singleIndirectBlocks = readBlockIntoArray(blockNo, super, fd);
    singleIndirectBlocks.insert(singleIndirectBlocks.begin(), blockNo);
    
    int count = singleIndirectBlocks.size();

    // print to screen
    printf("    #: %d", count);
    printf("    [");
    for (int i = 0; i < count; i++) printf("%d ", singleIndirectBlocks[i]);
    printf("]\n");

    return singleIndirectBlocks;
  } else {
    return {};
  }
}

// IN PROGRESS, return in if block missing
std::vector<int> printDoubleIndirectBlocks(int blockNo, int index,
                                 struct ext2_super_block super, int fd) {
    std::vector<int> allReachedFromDouble;
  if (blockNo) {
    std::vector<int> doubleBlock = readBlockIntoArray(blockNo, super, fd);
    int countOfDoubles = doubleBlock.size();

    for (int i = 0; i < countOfDoubles; i++) {
      int blockNoOfSingle = doubleBlock[i];

      std::vector<int> local = printSingleIndirectBlocks(blockNoOfSingle, index, super, fd);
      // local.insert(local.begin(), blockNoOfSingle);
      allReachedFromDouble.insert(allReachedFromDouble.end(), local.begin(), local.end());
    }

    allReachedFromDouble.insert(allReachedFromDouble.begin(), blockNo);
    return allReachedFromDouble;

  } else {
    return {};
  }
}

// IN PROGRESS, return in if block missing
std::vector<int> printTripleIndirectBlocks(int blockNo, int index,
                                 struct ext2_super_block super, int fd) {
    std::vector<int> allReachedFromTriple;
  if (blockNo) {
    std::vector<int> tripleBlock = readBlockIntoArray(blockNo, super, fd);
    int countOfTriples = tripleBlock.size();

    for (int i = 0; i < countOfTriples; i++) {
      int blockNoOfDouble = tripleBlock[i];
      std::vector<int> local = printDoubleIndirectBlocks(blockNoOfDouble, index, super, fd);
      allReachedFromTriple.insert(allReachedFromTriple.end(), local.begin(), local.end());
    }
    allReachedFromTriple.insert(allReachedFromTriple.begin(), blockNo);
  } else {
    return {};
  }
}

std::vector<int> isReachable(std::vector<int> blocksArray, bmap* blockBitmap) {
  int count = blocksArray.size();

  int reachableCount = 0;
  for (int i = 0; i < count; i++) {
    int blockIndex = blocksArray[i];
    // note the -1 part
    if (BM_ISSET(blockIndex - 1, blockBitmap)) {
      printf("REACHED: %d\n", blockIndex);
      reachableCount++;
    } else {
      printf("CANNOT: %d\n", blockIndex);
    }
  }
  if (reachableCount == count) {
    printf("[STATUS]: ALL Reachable\n");
    return blocksArray;
  } else {
    printf("[STATUS]: NOT REACHABLE\n");
    return {};
  }
}

std::vector<int> mergeAll(std::vector<int> List[])
{
    std::vector<int> allBlocksYouCanThinkOf;
    for(int i=0; i<4; i++)
    {
        std::vector<int> part = List[i];
        allBlocksYouCanThinkOf.insert(allBlocksYouCanThinkOf.begin(), part.begin(), part.end());
    }
    return allBlocksYouCanThinkOf;
}

std::vector<int> inode_block_reader(struct ext2_inode inode, int index,
                          struct ext2_super_block super, int fd) {
  printf("Inode No: %d\n", index);
  if (S_ISREG(inode.i_mode))
    printf("FILE, ");
  else if (S_ISDIR(inode.i_mode))
    printf("FOLDER, ");

  printf("Size: %d, #Blocks: %u\n", inode.i_size,
         inode.i_blocks / (block_size / 512));
  if (inode.i_size == 0) {
    printf("\n");
    return {};
  }

  std::vector<int> directBlocks = printDirectBlocks(inode, index);
  std::vector<int> singleIndirectBlocks =
      printSingleIndirectBlocks(inode.i_block[12], index, super, fd);
  std::vector<int> doubleIndirectBlocks =
      printDoubleIndirectBlocks(inode.i_block[13], index, super, fd);
  std::vector<int> tripleIndirectBlocks =
      printTripleIndirectBlocks(inode.i_block[14], index, super, fd);
  std::vector<int> List[] = {directBlocks, singleIndirectBlocks,
                         doubleIndirectBlocks, tripleIndirectBlocks};

  
  std::vector<int> allBlocks = mergeAll(List);

  return allBlocks;

  printf("\n");
}

int main(void) {
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

  printf("Reading from image file " IMAGE
         ":\n"
         "Blocks count            : %u\n"
         "First non-reserved inode: %u\n",
         super.s_blocks_count, super.s_first_ino);

  unsigned int group_count =
      1 + (super.s_blocks_count - 1) / super.s_blocks_per_group;
  unsigned int inode_table_count =
      sizeof(struct ext2_inode) * super.s_inodes_count / block_size;
  unsigned int datablocks_start = 1 + group_count + 1 + 1 + inode_table_count;
  printf("Data Blocks Start @: %u\n", datablocks_start + 1);

  // read group descriptor
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, &group, sizeof(group));
  printf("Block Bitmap : %d\n", group.bg_block_bitmap);
  printf("Inode Bitmap : %d\n", group.bg_inode_bitmap);
  printf("Inode Table  : %d\n", group.bg_inode_table);

  // read block bitmap
  bmap* block_bitmap;
  block_bitmap = (bmap*) malloc( block_size );
  lseek(fd, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
  read(fd, block_bitmap, block_size);

  bmap* inode_bitmap;
  inode_bitmap = (bmap*) malloc(block_size);
  lseek(fd, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
  read(fd, inode_bitmap, block_size);

  //// read root inode
  struct ext2_inode inode;
  int max = 0;
  for (int i = 0; i < super.s_first_ino; i++) {
    if (BM_ISSET(i, inode_bitmap)) {
      lseek(fd,
            BLOCK_OFFSET(group.bg_inode_table) + i * sizeof(struct ext2_inode),
            SEEK_SET);
      read(fd, &inode, sizeof(struct ext2_inode));
      for (int i = 0; i < 15; i++) {
        if (inode.i_block[i] > max) max = inode.i_block[i];
      }
    }
  }
  max += 1;
  printf("START FROM: %d\n", max);

  // read inode bitmap
  for (int i = super.s_first_ino; i < super.s_inodes_count; i++) {
    lseek(fd,
          BLOCK_OFFSET(group.bg_inode_table) + i * sizeof(struct ext2_inode),
          SEEK_SET);
    read(fd, &inode, sizeof(struct ext2_inode));
    printf("Flags: %d\n", inode.i_dtime);
    std::vector<int> ih = inode_block_reader(inode, i + 1, super, fd);
    isReachable(ih, block_bitmap);
  }
  printf("#####\n");
  bitmap_block_reader(block_bitmap, super);

  close(fd);
  return 0;
}
