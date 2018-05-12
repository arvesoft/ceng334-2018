#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <vector>
#include "ext2.h"

// my addition to see if it is file or folder
#include <sys/stat.h>

#define EXT2_S_IRUSR 0x0100
#define EXT2_S_IWUSR 0x0080
#define EXT2_S_IXUSR 0x0040
#define EXT2_S_IRGRP 0x0020
#define EXT2_S_IWGRP 0x0010
#define EXT2_S_IXGRP 0x0008
#define EXT2_S_IROTH 0x0004
#define EXT2_S_IWOTH 0x0002
#define EXT2_S_IXOTH 0x0001

#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
// #define IMAGE "image_3files_1deleted.img"
#define IMAGE "image.img"

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
  struct ext2_dir_entry* dir_entry;
};

int fd;
struct ext2_super_block super;
struct ext2_group_desc group;
bmap* block_bitmap;
bmap* inode_bitmap;

size_t realDirEntrySize(struct ext2_dir_entry* entry) {
  return (8 + entry->name_len + 3) & (~3);
}

bool isDeleted(struct ext2_inode& inode) {
  // std::cout << "DeletionControl" << "\n";
  // std::cout << inode.i_mode << " " << inode.i_dtime << " " << inode.i_size <<
  // std::endl;
  return inode.i_mode == 0 || inode.i_dtime;
}

void activateInode(struct ext2_inode& inode, unsigned int inode_no) {
  inode.i_flags = 0;
  inode.i_dtime = 0;
  inode.i_links_count = 1;
  inode.i_mode = EXT2_S_IFREG | EXT2_S_IRUSR;
  BM_SET(inode_no, inode_bitmap);
  group.bg_free_inodes_count--;
  super.s_free_inodes_count--;
  lseek(fd,
        BLOCK_OFFSET(group.bg_inode_table) +
            (inode_no - 1) * sizeof(struct ext2_inode),
        SEEK_SET);
  write(fd, &inode, sizeof(struct ext2_inode));
}

void markBlocksAsUsed(const std::vector<unsigned int>& blocks) {
  for (const unsigned int block : blocks) {
    BM_SET(block - 1, block_bitmap);
  }
  group.bg_free_blocks_count -= blocks.size();
  super.s_free_blocks_count -= blocks.size();
}

void bitmap_block_reader() {
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

std::vector<unsigned int> printDirectBlocks(struct ext2_inode inode,
                                            int index) {
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

  // printf("DIRECT:");
  // printf("    #: %d", count);
  // printf("    [");
  // for (int i = 0; i < count; i++)
  //   if (directBlocks[i] != -1) printf("%d ", directBlocks[i]);
  // printf("]\n");
  return directBlocks;
}

std::vector<unsigned int> readBlockIntoArray(int blockNo,
                                             struct ext2_super_block super,
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

std::vector<unsigned int> printSingleIndirectBlocks(
    int blockNo, int index, struct ext2_super_block super, int fd) {
  int num_of_ptrs = block_size / 4;

  // inode block 12 holds which inode holds directs
  if (blockNo) {
    // printf("SINGLE:");
    // printf("    SingleHolderBlock: %d", blockNo);

    std::vector<unsigned int> singleIndirectBlocks =
        readBlockIntoArray(blockNo, super, fd);
    singleIndirectBlocks.insert(singleIndirectBlocks.begin(), blockNo);

    int count = singleIndirectBlocks.size();

    // print to screen
    // printf("    #: %d", count);
    // printf("    [");
    // for (int i = 0; i < count; i++) printf("%d ", singleIndirectBlocks[i]);
    // printf("]\n");

    return singleIndirectBlocks;
  } else {
    return {};
  }
}

// IN PROGRESS, return in if block missing
std::vector<unsigned int> printDoubleIndirectBlocks(
    int blockNo, int index, struct ext2_super_block super, int fd) {
  std::vector<unsigned int> allReachedFromDouble;
  if (blockNo) {
    std::vector<unsigned int> doubleBlock =
        readBlockIntoArray(blockNo, super, fd);
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
std::vector<unsigned int> printTripleIndirectBlocks(
    int blockNo, int index, struct ext2_super_block super, int fd) {
  std::vector<unsigned int> allReachedFromTriple;
  if (blockNo) {
    std::vector<unsigned int> tripleBlock =
        readBlockIntoArray(blockNo, super, fd);
    int countOfTriples = tripleBlock.size();

    for (int i = 0; i < countOfTriples; i++) {
      int blockNoOfDouble = tripleBlock[i];
      std::vector<unsigned int> local =
          printDoubleIndirectBlocks(blockNoOfDouble, index, super, fd);
      allReachedFromTriple.insert(allReachedFromTriple.end(), local.begin(),
                                  local.end());
    }
    allReachedFromTriple.insert(allReachedFromTriple.begin(), blockNo);
    return allReachedFromTriple;
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
    if (!BM_ISSET(blockIndex - 1, block_bitmap)) {
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
    printf("FILE\n ");
  else if (S_ISDIR(inode.i_mode))
    printf("FOLDER\n ");

  // printf("Size: %d, #Blocks: %u\n", inode.i_size,
  //        inode.i_blocks / (block_size / 512));

  // if (inode.i_size == 0) {
  //   // printf("\n");
  //   return {};
  // }

  std::vector<unsigned int> directBlocks = printDirectBlocks(inode, 0);
  std::vector<unsigned int> singleIndirectBlocks =
      printSingleIndirectBlocks(inode.i_block[12], 0, super, fd);
  std::vector<unsigned int> doubleIndirectBlocks =
      printDoubleIndirectBlocks(inode.i_block[13], 0, super, fd);
  std::vector<unsigned int> tripleIndirectBlocks =
      printTripleIndirectBlocks(inode.i_block[14], 0, super, fd);
  std::vector<unsigned int> List[] = {directBlocks, singleIndirectBlocks,
                                      doubleIndirectBlocks,
                                      tripleIndirectBlocks};

  std::vector<unsigned int> allBlocks = mergeAll(List);

  std::cout << "getBlocks(): [";
  for (int i = 0; i < allBlocks.size(); i++) std::cout << allBlocks[i] << " ";
  std::cout << "]" << std::endl;

  return allBlocks;

  // printf("\n");
}

struct ext2_inode getInodeInfo(unsigned int inodeNo) {
  struct ext2_inode inode;
  lseek(fd,
        BLOCK_OFFSET(group.bg_inode_table) +
            (inodeNo - 1) * sizeof(struct ext2_inode),
        SEEK_SET);
  read(fd, &inode, sizeof(struct ext2_inode));
  printf("getInodeInfo(%d): %d %d %d %d %d\n", inodeNo, inode.i_size,
         inode.i_mode, inode.i_blocks, inode.i_flags, inode.i_links_count);
  return inode;
}

std::vector<struct ext2_dir_entry*> getChildren(struct ext2_inode inode) {
  unsigned char block[block_size];
  lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
  read(fd, block, block_size);

  std::vector<struct ext2_dir_entry*> directoryEntries;

  unsigned int size = 0;
  struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;
  while (size < inode.i_size && entry->inode) {
    const size_t real_len = realDirEntrySize(entry);
    struct ext2_dir_entry* new_entry = (struct ext2_dir_entry*)malloc(real_len);
    std::memcpy(new_entry, entry, real_len);
    printf("    getChildren: %10u %10u %.*s\n", entry->inode, entry->rec_len,
           entry->name_len, entry->name);

    directoryEntries.push_back(new_entry);

    size += entry->rec_len;
    entry =
        (ext2_dir_entry*)((void*)entry + real_len); /* move to the next entry */
  }

  return directoryEntries;
}

bool isInTheVector(const std::vector<unsigned int>& vector,
                   unsigned int element) {
  for (int i = 0; i < vector.size(); i++) {
    if (vector[i] == element) return true;
  }
  return false;
}

void putInodeInto(struct ext2_dir_entry* entry_to_put,
                  struct ext2_inode& lost_found_inode) {
  /// printf("BBB: %d %d %d\n", lost_found_inode.i_size,
  /// lost_found_inode.i_mode, lost_found_inode.i_blocks);
  unsigned char block[block_size];
  lseek(fd, BLOCK_OFFSET(lost_found_inode.i_block[0]), SEEK_SET);
  read(fd, block, block_size);

  unsigned int size = 0;
  unsigned int previousSize = 0;
  const size_t needed_size = realDirEntrySize(entry_to_put);
  struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;
  while (size < lost_found_inode.i_size && entry->inode) {
    printf("    putInodeInto: %10u %10u %.*s\n", entry->inode, entry->rec_len,
           entry->name_len, entry->name);

    const size_t real_len = realDirEntrySize(entry);
    if (entry->rec_len - real_len >= needed_size) {
      struct ext2_dir_entry* new_entry =
          (struct ext2_dir_entry*)((void*)entry + real_len);
      memcpy(new_entry, entry_to_put, needed_size);
      new_entry->rec_len = entry->rec_len - real_len;
      entry->rec_len = real_len;
      break;
    }
    size += entry->rec_len;
    entry = (ext2_dir_entry*)((void*)entry +
                              entry->rec_len); /* move to the next entry */
  }

  lseek(fd, BLOCK_OFFSET(lost_found_inode.i_block[0]), SEEK_SET);
  write(fd, block, block_size);
}

int main(void) {
  if ((fd = open(IMAGE, O_RDWR)) < 0) {
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

  std::unordered_set<unsigned int> visited;
  std::queue<struct ext2_dir_entry*> to_visit;
  std::vector<struct ext2_dir_entry*> to_recover;
  to_visit.push(&root_entry);

  bitmap_block_reader();

  while (!to_visit.empty()) {
    const auto dir_entry = to_visit.front();
    to_visit.pop();
    // add to visited inode no; so it does not loop to infinity
    visited.insert(dir_entry->inode);
    struct ext2_inode inode = getInodeInfo(dir_entry->inode);
    if (S_ISDIR(inode.i_mode)) {
      std::vector<struct ext2_dir_entry*> children = getChildren(inode);
      for (const auto child : children) {
        // if it is visited before, do not add to queue
        if (visited.find(child->inode) != visited.end()) continue;
        to_visit.push(child);
      }
    } else {
      if (isDeleted(inode)) {
        printf("%.*s\n", dir_entry->name_len, dir_entry->name);
        std::vector<unsigned int> blocks = GetBlocks(inode);
        if (isReachable(blocks)) {
          to_recover.push_back(dir_entry);
        }
      }
    }
    std::cout << std::endl;
  }

  std::cout << "#\n";

  std::sort(to_recover.begin(), to_recover.end());

  struct ext2_inode lost_found_inode = getInodeInfo(11);
  for (const auto dir_entry : to_recover) {
    printf("%.*s\n", dir_entry->name_len, dir_entry->name);
    struct ext2_inode inode = getInodeInfo(dir_entry->inode);
    std::vector<unsigned int> blocks = GetBlocks(inode);
    activateInode(inode, dir_entry->inode);
    putInodeInto(dir_entry, lost_found_inode);
    markBlocksAsUsed(blocks);
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
