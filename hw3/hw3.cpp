#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <queue>
#include <vector>
#include "ext2.h"
#include <cstring>

// my addition to see if it is file or folder
#include <sys/stat.h>

#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
#define IMAGE "image_3files_1deleted.img"
// #define IMAGE "image_indirect.img"

typedef unsigned char bmap;
#define __NBITS (8 * (int)sizeof(bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap)1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT(d)] |= __BMMASK(d)))
#define BM_CLR(d, set) ((set[__BMELT(d)] &= ~__BMMASK(d)))
#define BM_ISSET(d, set) ((set[__BMELT(d)] & __BMMASK(d)) != 0)

unsigned int block_size = 0;
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

struct dir_entry_with_parent {
  unsigned int parent_inode_number;
  struct ext2_dir_entry dir_entry;
};

int fd;
struct ext2_super_block super;
struct ext2_group_desc group;
bmap* block_bitmap;
bmap* inode_bitmap;

bool isDeleted(struct ext2_inode& inode) {
  return inode.i_mode == 0 || inode.i_dtime;
}

void activateInode(struct ext2_inode& inode, unsigned int inode_no) {
  inode.i_dtime = 0;
  inode.i_mode = EXT2_FT_REG_FILE;
  BM_SET(inode_no, inode_bitmap);
  group.bg_free_inodes_count--;
  super.s_free_inodes_count--;
}

void markBlocksAsUsed(const std::vector<unsigned int>& blocks) {
  for (const unsigned int block : blocks) {
    BM_SET(block - 1, block_bitmap);
  }
  group.bg_free_blocks_count -= blocks.size();
  super.s_free_blocks_count -= blocks.size();
}

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

std::vector<unsigned int> printDirectBlocks(struct ext2_inode inode, int index) {
  std::vector<unsigned int> directBlocks;

  int count = 0;
  for (int i = 0; i < 12; i++) {
    if (!(inode.i_block[i]))
      continue;
    else  // direct blocks
    {
      count++;
      directBlocks.push_back(inode.i_block[i]);
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

std::vector<unsigned int> readBlockIntoArray(int blockNo, struct ext2_super_block super,
                                    int fd) {
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
    std::vector<unsigned int> singleIndirectBlocks;
    singleIndirectBlocks.resize(0);

    // move cursor back to its position, then read into array
    lseek(fd,
          BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1),
          SEEK_SET);
    for (int i = 0; i < num_of_ptrs; i++) {
      read(fd, &arr, 4);
      if (arr == 0) break;
      singleIndirectBlocks.push_back(arr);
    }

    return singleIndirectBlocks;
  } else {
    // SCREAM HERE
    return {};
  }
}

std::vector<unsigned int> printSingleIndirectBlocks(int blockNo, int index,
                                           struct ext2_super_block super,
                                           int fd) {
  int num_of_ptrs = block_size / 4;

  // inode block 12 holds which inode holds directs
  if (blockNo) {
    printf("SINGLE:");
    printf("    SingleHolderBlock: %d", blockNo);

    std::vector<unsigned int> singleIndirectBlocks =
        readBlockIntoArray(blockNo, super, fd);
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
std::vector<unsigned int> printDoubleIndirectBlocks(int blockNo, int index,
                                           struct ext2_super_block super,
                                           int fd) {
  std::vector<unsigned int> allReachedFromDouble;
  if (blockNo) {
    std::vector<unsigned int> doubleBlock = readBlockIntoArray(blockNo, super, fd);
    int countOfDoubles = doubleBlock.size();

    for (int i = 0; i < countOfDoubles; i++) {
      int blockNoOfSingle = doubleBlock[i];

      std::vector<unsigned int> local =
          printSingleIndirectBlocks(blockNoOfSingle, index, super, fd);
      // local.insert(local.begin(), blockNoOfSingle);
      allReachedFromDouble.insert(allReachedFromDouble.end(), local.begin(),
                                  local.end());
    }

    allReachedFromDouble.insert(allReachedFromDouble.begin(), blockNo);
    return allReachedFromDouble;

  } else {
    return {};
  }
}

// IN PROGRESS, return in if block missing
std::vector<unsigned int> printTripleIndirectBlocks(int blockNo, int index,
                                           struct ext2_super_block super,
                                           int fd) {
  std::vector<unsigned int> allReachedFromTriple;
  if (blockNo) {
    std::vector<unsigned int> tripleBlock = readBlockIntoArray(blockNo, super, fd);
    int countOfTriples = tripleBlock.size();

    for (int i = 0; i < countOfTriples; i++) {
      int blockNoOfDouble = tripleBlock[i];
      std::vector<unsigned int> local =
          printDoubleIndirectBlocks(blockNoOfDouble, index, super, fd);
      allReachedFromTriple.insert(allReachedFromTriple.end(), local.begin(),
                                  local.end());
    }
    allReachedFromTriple.insert(allReachedFromTriple.begin(), blockNo);
  } else {
    return {};
  }
}

bool isReachable(const std::vector<unsigned int>& blocksArray) {
  int count = blocksArray.size();

  int reachableCount = 0;
  for (int i = 0; i < count; i++) {
    int blockIndex = blocksArray[i];
    // note the -1 part
    if (BM_ISSET(blockIndex - 1, block_bitmap)) {
      printf("REACHED: %d\n", blockIndex);
      reachableCount++;
    } else {
      printf("CANNOT: %d\n", blockIndex);
    }
  }
  if (reachableCount == count) {
    printf("[STATUS]: ALL Reachable\n");
    return true;
  }
  printf("[STATUS]: NOT REACHABLE\n");
  return false;
}

std::vector<unsigned int> mergeAll(std::vector<unsigned int> List[]) {
  std::vector<unsigned int> allBlocksYouCanThinkOf;
  for (int i = 0; i < 4; i++) {
    std::vector<unsigned int> part = List[i];
    allBlocksYouCanThinkOf.insert(allBlocksYouCanThinkOf.begin(), part.begin(),
                                  part.end());
  }
  return allBlocksYouCanThinkOf;
}

std::vector<unsigned int> GetBlocks(struct ext2_inode inode) {
  // printf("Inode No: %d\n", index);
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

  std::vector<unsigned int> directBlocks = printDirectBlocks(inode, 0);
  std::vector<unsigned int> singleIndirectBlocks =
      printSingleIndirectBlocks(inode.i_block[12], 0, super, fd);
  std::vector<unsigned int> doubleIndirectBlocks =
      printDoubleIndirectBlocks(inode.i_block[13], 0, super, fd);
  std::vector<unsigned int> tripleIndirectBlocks =
      printTripleIndirectBlocks(inode.i_block[14], 0, super, fd);
  std::vector<unsigned int> List[] = {directBlocks, singleIndirectBlocks,
                             doubleIndirectBlocks, tripleIndirectBlocks};

  std::vector<unsigned int> allBlocks = mergeAll(List);

  return allBlocks;

  printf("\n");
}

struct ext2_inode getInodeInfo(unsigned int inodeNo)
{
  struct ext2_inode inode;
  lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(inodeNo-1)*sizeof(struct ext2_inode), SEEK_SET);
  read(fd, &inode, sizeof(struct ext2_inode));
  printf("AAA: %d %d %d\n", inode.i_size, inode.i_mode, inode.i_blocks);
  return inode;
}

std::vector<struct ext2_dir_entry> getChildren(struct ext2_inode inode)
{
  printf("BBB: %d %d %d\n", inode.i_size, inode.i_mode, inode.i_blocks);
  unsigned char block[block_size];
  lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
  read(fd, block, block_size);

  std::vector<struct ext2_dir_entry> directoryEntries;

  unsigned int size = 0;
  struct ext2_dir_entry *entry = (struct ext2_dir_entry *) block;
  while(size < inode.i_size && entry->inode)
  {
        
        char file_name[EXT2_NAME_LEN+1];
        std::memcpy(file_name, entry->name, entry->name_len);
        file_name[entry->name_len] = 0;              /* append null char to the file name */
        printf("aaa: %10u %s\n", entry->inode, file_name);

        ext2_dir_entry dirEntry = *entry;
        directoryEntries.push_back(dirEntry);

        size += entry->rec_len;
        entry = (ext2_dir_entry*) entry + entry->rec_len;      /* move to the next entry */
        
}

  return directoryEntries;
}

int main(void) {
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

  unsigned int group_count =
      1 + (super.s_blocks_count - 1) / super.s_blocks_per_group;

  // read block bitmap
  block_bitmap = new bmap[block_size];
  lseek(fd, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
  read(fd, block_bitmap, block_size);

  inode_bitmap = new bmap[block_size];
  lseek(fd, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
  read(fd, inode_bitmap, block_size);

  struct ext2_dir_entry root_entry;
  root_entry.inode = 2;

  std::queue<dir_entry_with_parent> to_visit;
  std::queue<dir_entry_with_parent> to_recover;
  to_visit.push({0, root_entry});

  while (!to_visit.empty()) {
    const auto front = to_visit.front();
    const auto dir_entry = front.dir_entry;
    to_visit.pop();
    struct ext2_inode inode = getInodeInfo(dir_entry.inode);
    if (S_ISDIR(inode.i_mode)) {
      std::vector<struct ext2_dir_entry> children = getChildren(inode);
      for (const auto& child : children) {
        to_visit.push({dir_entry.inode, child});
        std::cout << "CHECK: " << dir_entry.name << std::endl;
      }
    } else {
      if (isDeleted(inode)) {
        std::cout << dir_entry.name << '\n';
        std::vector<unsigned int> blocks = GetBlocks(inode);
        if (isReachable(blocks)) {
          to_recover.push(front);
        }
      }
    }
  }

  std::cout << "#\n";

  struct ext2_inode lost_found_inode = getInodeInfo(11);
  while (!to_recover.empty()) {
    const auto front = to_recover.front();
    const auto dir_entry = front.dir_entry;
    std::cout << dir_entry.name << '\n';
    to_recover.pop();
    struct ext2_inode inode = getInodeInfo(dir_entry.inode);
    struct ext2_inode parent_inode = getInodeInfo(front.parent_inode_number);
    // std::vector<unsigned int> blocks = GetBlocks(inode);
    // activateInode(inode);
    // putInodeInto(inode, lost_found_inode);
    // deleteInodeFrom(inode, parent_inode);
    // markBlocksAsUsed(blocks);
  }

  lseek(fd, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
  write(fd, inode_bitmap, block_size);

  lseek(fd, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
  write(fd, block_bitmap, block_size);

  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  write(fd, &group, sizeof(group));

  lseek(fd, BASE_OFFSET, SEEK_SET);
  write(fd, &super, sizeof(super));

  close(fd);
  return 0;
}
