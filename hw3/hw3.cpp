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

size_t realDirEntrySize(unsigned int name_len) {
  return (8 + name_len + 3) & (~3);
}

size_t realDirEntrySize(struct ext2_dir_entry* entry) {
  return (8 + entry->name_len + 3) & (~3);
}

bool isDeleted(const struct ext2_inode& inode) {
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

std::vector<unsigned int> printDirectBlocks(struct ext2_inode inode) {
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
    int blockNo, struct ext2_super_block super, int fd) {
  // inode block 12 holds which inode holds directs
  if (blockNo) {
    // printf("SINGLE:");
    // printf("    SingleHolderBlock: %d", blockNo);

    std::vector<unsigned int> singleIndirectBlocks =
        readBlockIntoArray(blockNo, super, fd);
    singleIndirectBlocks.insert(singleIndirectBlocks.begin(), blockNo);

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
    int blockNo, struct ext2_super_block super, int fd) {
  std::vector<unsigned int> allReachedFromDouble;
  if (blockNo) {
    std::vector<unsigned int> doubleBlock =
        readBlockIntoArray(blockNo, super, fd);
    int countOfDoubles = doubleBlock.size();

    for (int i = 0; i < countOfDoubles; i++) {
      int blockNoOfSingle = doubleBlock[i];

      std::vector<unsigned int> local =
          printSingleIndirectBlocks(blockNoOfSingle, super, fd);
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
    int blockNo, struct ext2_super_block super, int fd) {
  std::vector<unsigned int> allReachedFromTriple;
  if (blockNo) {
    std::vector<unsigned int> tripleBlock =
        readBlockIntoArray(blockNo, super, fd);
    int countOfTriples = tripleBlock.size();

    for (int i = 0; i < countOfTriples; i++) {
      int blockNoOfDouble = tripleBlock[i];
      std::vector<unsigned int> local =
          printDoubleIndirectBlocks(blockNoOfDouble, super, fd);
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

  std::vector<unsigned int> directBlocks = printDirectBlocks(inode);
  std::vector<unsigned int> singleIndirectBlocks =
      printSingleIndirectBlocks(inode.i_block[12], super, fd);
  std::vector<unsigned int> doubleIndirectBlocks =
      printDoubleIndirectBlocks(inode.i_block[13], super, fd);
  std::vector<unsigned int> tripleIndirectBlocks =
      printTripleIndirectBlocks(inode.i_block[14], super, fd);
  std::vector<unsigned int> List[] = {directBlocks, singleIndirectBlocks,
                                      doubleIndirectBlocks,
                                      tripleIndirectBlocks};

  std::vector<unsigned int> allBlocks = mergeAll(List);

  std::cout << "getBlocks(): [";
  for (size_t i = 0; i < allBlocks.size(); i++)
    std::cout << allBlocks[i] << " ";
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

void putInodeInto(struct ext2_dir_entry* entry_to_put,
                  struct ext2_inode& lost_found_inode) {
  /// printf("BBB: %d %d %d\n", lost_found_inode.i_size,
  /// lost_found_inode.i_mode, lost_found_inode.i_blocks);
  unsigned char block[block_size];
  lseek(fd, BLOCK_OFFSET(lost_found_inode.i_block[0]), SEEK_SET);
  read(fd, block, block_size);

  unsigned int size = 0;
  const size_t needed_size = realDirEntrySize(entry_to_put);
  struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;
  while (size < lost_found_inode.i_size && entry->inode) {
    printf("    putInodeInto: %10u %10u %.*s\n", entry->inode, entry->rec_len,
           entry->name_len, entry->name);

    const size_t real_len = realDirEntrySize(entry);
    if (entry->rec_len - real_len >= needed_size) {
      struct ext2_dir_entry* new_entry =
          (struct ext2_dir_entry*)((char*)entry + real_len);
      memcpy(new_entry, entry_to_put, needed_size);
      new_entry->rec_len = entry->rec_len - real_len;
      entry->rec_len = real_len;
      break;
    }
    size += entry->rec_len;
    entry = (ext2_dir_entry*)((char*)entry +
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
  std::vector<std::pair<unsigned int, struct ext2_dir_entry*>> to_recover;
  to_visit.push(&root_entry);

  size_t deleted_file_count = 0;
  size_t digit_count = 1;
  size_t digit_limit = 10;
  for (size_t i = 0; i < super.s_inodes_per_group; i++) {
    const unsigned int inode_no = super.s_first_ino + i;
    const struct ext2_inode inode = getInodeInfo(inode_no);
    if (S_ISREG(inode.i_mode)) {
      if (isDeleted(inode)) {
        deleted_file_count++;
        if (deleted_file_count >= digit_limit) {
          digit_limit *= 10;
          digit_count++;
        }
        struct ext2_dir_entry* dir_entry =
            (struct ext2_dir_entry*)malloc(realDirEntrySize(4 + digit_count));
        sprintf(dir_entry->name, "file%zu", deleted_file_count);
        dir_entry->name_len = 4 + digit_count;
        dir_entry->inode = inode_no;
        dir_entry->file_type = EXT2_FT_REG_FILE;
        printf("%.*s\n", dir_entry->name_len, dir_entry->name);

        const std::vector<unsigned int> blocks = GetBlocks(inode);
        if (isReachable(blocks)) {
          to_recover.push_back({inode.i_dtime, dir_entry});
        }
      }
    }
  }
  std::cout << "#\n";

  std::make_heap(to_recover.begin(), to_recover.end());

  struct ext2_inode lost_found_inode = getInodeInfo(11);
  while (!to_recover.empty()) {
    const auto front = to_recover.front();
    std::pop_heap(to_recover.begin(), to_recover.end());
    to_recover.pop_back();

    const auto dir_entry = front.second;
    struct ext2_inode inode = getInodeInfo(dir_entry->inode);
    const std::vector<unsigned int> blocks = GetBlocks(inode);
    if (!isReachable(blocks)) {
      continue;
    }

    printf("dtime:%u %.*s\n", front.first, dir_entry->name_len,
           dir_entry->name);
    activateInode(inode, dir_entry->inode);
    putInodeInto(dir_entry, lost_found_inode);
    markBlocksAsUsed(blocks);
    free(dir_entry);
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
