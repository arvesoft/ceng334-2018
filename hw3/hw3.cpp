#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <vector>
#include <queue>
#include <cstring>
#include "ext21.h"
using namespace std;
#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
unsigned int block_size = 0;
typedef unsigned char bmap;
#define __NBITS (8 * (int)sizeof(bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap)1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT(d)] |= __BMMASK(d)))
#define BM_CLR(d, set) ((set[__BMELT(d)] &= ~__BMMASK(d)))
#define BM_ISSET(d, set) ((set[__BMELT(d)] & __BMMASK(d)) != 0)
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

struct ext2_super_block *super;
struct ext2_group_desc *group;
struct ext2_inode *inode;
struct stat sb, fileStat;
vector<unsigned int> blockNumbers;
unsigned int targetNode = 0;
unsigned int blocksPerGroup = 0;
unsigned int groupNo = 0;
unsigned int groupCount = 0;
unsigned int inodePad = 0;
unsigned int inodeNew = 0;
int fd, fd2;
bool inodeFoundFLag = false;
char nameCache[256];
char *addr;
unsigned int sizeSource;
unsigned int neededBlock;

//*************************************************************************
//*************************************************************************
//*************************************************************************

void getAllBlocks(vector<unsigned int> *blocks, struct ext2_inode *inode)
{
    unsigned int usedNodes = 0;
    unsigned int j = 0;
    blocks->clear();
    for (; j < 12; j++)
    {
        //cout << "check: "<<j<<endl;
        if (inode->i_block[j])
        {
            //cout << "bu blockta: "<< inode ->i_block[j] << endl;
            blocks->push_back(inode->i_block[j]);
            usedNodes++;
        }
    }

    // for bonus continue from here to indirect blocks
}

struct ext2_inode *getInodeAddress(unsigned int inodeNo)
{
    return (struct ext2_inode *)(BLOCK_OFFSET(group->bg_inode_table) +
                                 ((inodeNo - 1) % blocksPerGroup) * sizeof(struct ext2_inode));
}

unsigned int getFreeInode(unsigned int groupNum)
{
    struct ext2_group_desc *group = (struct ext2_group_desc *)(addr + BASE_OFFSET + block_size + (groupNum) * sizeof(struct ext2_group_desc));

    bmap *inodeMap = (bmap *)(addr + BASE_OFFSET + 2*block_size + (groupNum) * sizeof(struct ext2_group_desc));
    cout << "funka girmeden once: " << super->s_inodes_per_group<<"---"<<group->bg_inode_bitmap<<endl;
    //burada offset sıkıntısı var burayi duzelt olmai manuel free node bul sonra don
    //buraya kadar sıkıntı yok gibi gözüküyor iyice kotrol at yine de ama
    unsigned int i = 0;
    cout <<"super inode:"<< inode <<" per group: "<<inodeMap<<"   "<<group->bg_inode_bitmap <<endl;
    //return 367;
    for (i = 0; i < super->s_inodes_per_group; i++)
    {
        if (!BM_ISSET(i, inodeMap))
        {
            BM_SET(i, inodeMap);
            super->s_free_inodes_count--;
            group->bg_free_inodes_count--;
            
            cout<<"bu inode alindi: "<<i + super->s_inodes_per_group * groupNum + 1<<endl;
            return i + super->s_inodes_per_group * groupNum + 1;
        }
    }
    return getFreeInode(groupNum+1);

    inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + sizeof(struct ext2_inode));
}

queue<unsigned int> getNBlock(unsigned int sourceFileBlockCount, unsigned int groupNum)
{
   
    group = (struct ext2_group_desc *)(addr + BASE_OFFSET + block_size + (groupNum) * sizeof(struct ext2_group_desc));
    bmap *blockMap = (bmap *)(addr + BLOCK_OFFSET(group->bg_block_bitmap));
    unsigned int start = BLOCK_OFFSET(group->bg_inode_table)+inodePad;
    unsigned int i = start%super->s_blocks_per_group -1;
    unsigned int directLimit = 12;
    queue<unsigned int> stack;
    //queue<unsigned int> stackToAppend;
    cout<<"hoop"<<endl;
    for (; i < super->s_blocks_per_group; i++)
    {
        cout<< "check this i: "<<i<<endl;
        start++;
        if (!BM_ISSET(i, blockMap))
        {
            cout<<"set this i:"<<i<<endl;
            BM_SET(i, blockMap);
            stack.push(start);
            sourceFileBlockCount--;
            directLimit--;
            group->bg_free_blocks_count--;
            super->s_free_blocks_count--;
            if (directLimit == 0||sourceFileBlockCount == 0 )
            {
                break;
            }
            
        }
    }

    
    /*
    if (sourceFileBlockCount)
    {
        stackToAppend = getNBlock(sourceFileBlockCount,groupCount);
        while (!stackToAppend.empty())
        {
            stack.push(stackToAppend.front());
            stackToAppend.pop();
        }
        
    }
    */
    return stack;
}


void fillBlockPointers(queue<unsigned int>reservedBlocks,struct ext2_inode *inode){
    unsigned int sizeOf = reservedBlocks.size();
    unsigned int putCount = 0;
    while (putCount <12)
    {
        inode->i_block[putCount] = reservedBlocks.front();
        reservedBlocks.pop();
        putCount++;
    }
    

}
//*************************************************************************
//*************************************************************************
//*************************************************************************

int main(int argc, char **argv)
{
    
    unsigned int targetNode = 0;
    unsigned int blocksPerGroup = 0;
    unsigned int groupNo = 0;
    unsigned int groupCount = 0;
    bool inodeFoundFLag = false;
    char nameCache[256];
    

    
    cout << "size: " << sizeof(int) << endl;
    if ((fd = open(argv[1], O_RDWR)) < 0)
    {
        perror("image");
        exit(1);
    }
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat error");
    }

    addr = (char *)mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        perror("burda");
    // read super-block

    super = (struct ext2_super_block *)(addr + BASE_OFFSET);
    if (super->s_magic != EXT2_SUPER_MAGIC)
    {
        perror("NOT EXT2");
        exit(1);
    }
    // some useful globals

    //******************************************************************************************************************
    //************ FIND INODE*******************************************************************************************
    //******************************************************************************************************************
    block_size = 1024 << super->s_log_block_size;
    blocksPerGroup = super->s_blocks_per_group;
    inodePad = (super->s_inodes_per_group - 1) * 118 / 1024 + 1;

    char targetDir[256];
    char i = 0;

    for (; i < argv[3][i] != '\0'; i++)
    {
        targetDir[i] = argv[3][i];
    }
    targetDir[i] = '\0';
    cout << targetDir << endl;
    int index = 0;
    unsigned int inodeCache = 2;
    unsigned int inodeNo = 2;
    struct ext2_dir_entry *entry;
    if (targetDir[0] == '/')
    {

        while (targetDir[index] != '\0')
        {
            int i = 0;
            char dest[100];
            bool inodeFoundFLag = false;
            for (; targetDir[i + index] != '\0' && targetDir[i + index] != '/'; i++)
            {
                nameCache[i] = targetDir[i + index];
            }
            cout << "check i:" << i << endl;
            if (i == 0)
            {

                nameCache[i] = '/';
                nameCache[i + 1] = '\0';
            }
            else
            {
                nameCache[i] = '\0';
            }
            strcpy(dest, nameCache);

            if (dest[0] == '/')
            {
                inodeNo = 2;
                index += (i + 1);
                continue;
            }
            else
                inodeNo = inodeCache;
            
            unsigned int groupOfBlock = 0;
            groupNo = (inodeNo - 1) / (super->s_inodes_per_group);
            group = (struct ext2_group_desc *)(addr + BASE_OFFSET + block_size + (groupNo) * sizeof(struct ext2_group_desc));
            inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + ((inodeNo - 1) % super->s_inodes_per_group) * sizeof(struct ext2_inode));
            getAllBlocks(&blockNumbers, inode);


            //simdi bulugumuz inodeun blocklarindaki ilk bos uygun yere yeni entry girmemiz lazim
            //yaptiktan sonra içi boş olsa da file managerden girince adi gozukmesi gerek
            //ek olarak su ana kadar yaptigimiz seyler sadece okumaydi bundan sonra image e bir seyler yazmaya baslicaz gerekli yerleri doldurmamiz lazim
            for (auto j = blockNumbers.begin(); j != blockNumbers.end() && inodeFoundFLag == false; ++j)
            {
                unsigned int readByte = 0;
                unsigned char block[block_size];
                entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(*j));

                while (readByte < inode->i_size)
                {
                    cout<<"ilk harf"<<entry->name[0]<<endl;
                    if (i == entry->name_len)
                    {
                        char file_name[EXT2_NAME_LEN + 1];
                        memcpy(file_name, entry->name, entry->name_len);
                        file_name[entry->name_len] = 0;

                        if (strcmp(file_name, dest) == 0)
                        {
                            inodeCache = entry->inode;
                            inodeFoundFLag = true;

                            cout << "inode FOUND: " << inodeCache << endl;
                            if (targetDir[index + i] == '\0')
                            {
                                inodeNo = inodeCache;
                                groupNo = (inodeNo - 1) / (super->s_inodes_per_group);
                                
                            }
                            break;
                        }
                    }
                    readByte += entry->rec_len;
                    entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(*j) + readByte);
                }
            }
            inodeFoundFLag = false;
            index += (i + 1);
        }
    }
    else
    {
        inodeNo = atoi(argv[2]);
        groupNo = (inodeNo - 1) / (super->s_inodes_per_group);
        group = (struct ext2_group_desc *)(addr + BASE_OFFSET + block_size + groupNo * sizeof(struct ext2_group_desc));
        inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + sizeof(struct ext2_inode));
        getAllBlocks(&blockNumbers, inode);
    }

    group = (struct ext2_group_desc *)(addr + BASE_OFFSET + block_size + (groupNo) * sizeof(struct ext2_group_desc));
    cout<<"-----"<<inodeNo<<"---\n";
    inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + ((inodeNo - 1) % super->s_inodes_per_group) * sizeof(struct ext2_inode));
    getAllBlocks(&blockNumbers, inode); // to place new directory entry
    //******************************************************************************************************************
    //************ FIND INODE*******************************************************************************************
    //******************************************************************************************************************

    // CHECK IF SOURCE FILE IS REGULAR OR DIRECTORY
    
    if (fileStat.st_mode == 0)
    {
        sizeSource = fileStat.st_size;
        neededBlock = sizeSource / block_size;
        // groupNo is known
        //
    }

    if ((fd2 = open(argv[2], O_RDONLY)) < 0)
    {
        perror("source ile");
        exit(1);
    }
    if (fstat(fd2, &fileStat) == -1)
    {
        perror("fstat error");
    }

    //file inode metadata
    int fileMode = fileStat.st_mode;
    char fileNameFirst[256];
    char a = 0;
    unsigned int record;
    unsigned int sourceFileSize = fileStat.st_size;
    unsigned int sourceFileBlockCount;
    
    if (sourceFileSize % block_size)
    {
        sourceFileBlockCount = sourceFileSize / block_size + 1;
    }
    else
    {
        sourceFileBlockCount = sourceFileSize / block_size;
    }

    for (; argv[2][a] != '\0'; a++)
    {
        fileNameFirst[a] = argv[2][a];
    }
    unsigned int cacheBlock;
    record = (a + (4 - a % 4)) + 8;
    cout << "------------------\n";
    bool filePut = false;
    for (auto j = blockNumbers.begin(); j != blockNumbers.end() && filePut == false; ++j)
    {
        unsigned int readByte = 0;
        unsigned char block[block_size];
        entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(*j));
        while (readByte < inode->i_size)
        {
            cout<<"bunun ilki: "<<entry->name[0]<<endl;
            if (block_size == readByte + entry->rec_len)
            {
                int recordLast = (entry->name_len + (4 - entry->name_len % 4)) + 8;
                int diff = entry->rec_len - recordLast;
                cout<<"here is rec len: "<<entry->rec_len<<endl;
                if (diff >= record)
                { //block will be put there
                    entry->rec_len = recordLast;
                    entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(*j) + readByte + recordLast);
                    entry->file_type = fileMode;
                    entry->name_len = a;
                    entry->rec_len = diff;
                    record = diff+1;
                    for (unsigned int i = 0; i < a; i++)
                    {
                        entry->name[i] = fileNameFirst[i];
                    }
                    cout << "funka girmeden once: " << super->s_inodes_per_group<<"---"<<group->bg_inode_bitmap<<endl;
                    entry->inode = getFreeInode(groupNo);
                    super->s_free_inodes_count--;
                    inodeNo = entry->inode;
                    inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + ((inodeNo - 1) % super->s_inodes_per_group) * sizeof(struct ext2_inode));
                    cacheBlock = *j;
                    inodeNew = entry->inode;
                    filePut = true;
                    break;
                }
            }
            readByte += entry->rec_len;
            entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(*j) + readByte);
        }
    }
        unsigned int readByte = 0;
        unsigned char block[block_size];
        entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(cacheBlock));
        while (readByte < inode->i_size)
        {
            inode->i_block[0];
            int recordLast = (entry->name_len + (4 - entry->name_len % 4)) + 8;
            int diff = entry->rec_len - recordLast;
            cout<<"here--"<< entry->name[0]<<" is rec len: "<<entry->rec_len<<endl;
            if (diff >= record)
            { //block will be put there
                entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(cacheBlock) + readByte + recordLast);
                record = diff+1;
                cout<<"buradan bak: ";
                for (unsigned int i = 0; i < a; i++)
                {
                    cout<<entry->name[i];
                }
                cout<<endl;

            }
            readByte += entry->rec_len;
            entry = (struct ext2_dir_entry *)(addr + BLOCK_OFFSET(cacheBlock) + readByte);
        }
    
    cout << inode->i_mode<<endl;
    inode->i_mode = 0;
    
    inode->i_uid = fileStat.st_uid;
    inode->i_size = fileStat.st_size;
    
    inode->i_atime = fileStat.st_atim.tv_sec;
    inode->i_ctime = fileStat.st_ctim.tv_sec;
    inode->i_mtime = fileStat.st_mtim.tv_sec;
    inode->i_gid = fileStat.st_gid;
    inode->i_links_count = fileStat.st_nlink;
    inode->i_blocks = sourceFileBlockCount;
    // get n free block from image and start filling
    // file size sayisi kadar inode a block bulan bi fonk güzel olabilir
    // boylece iki vector olur birine yazıp datayı öbürüne onun adresini atarım sürekli ya da direk birini digerine atayıp onun blocklarına da yazabilirim
    // indirect kisimda  ulasmak icin 1024 lük uint arrayi tanimlayip ona da atama yapmak gerek,

    queue<unsigned int> reservedBlocks = getNBlock(sourceFileBlockCount, groupNo);


    fillBlockPointers(reservedBlocks,inode);

    char buffer[block_size];
    unsigned int hasRead = 0;
    unsigned int limitter = 12;
    unsigned int counter = 0;
    while (hasRead < sourceFileSize && counter<12)
    {
        lseek(fd2, hasRead, SEEK_SET);
        read(fd2, buffer, block_size);

        lseek(fd,BLOCK_OFFSET(reservedBlocks.front()),SEEK_SET);
        write(fd,buffer,1024);
        
        hasRead+= block_size;
        counter++;
        reservedBlocks.pop();

            
    }
    
    








    // read block bitmap
    bmap *bitmap = (bmap *)(addr + BLOCK_OFFSET(group->bg_block_bitmap));
    /*int fr = 0;
    int nfr = 0;
    cout << "Free block bitmap:\n";
    for (int i = 0; i < super -> s_blocks_count; i++){
        if (BM_ISSET(i,bitmap)){
            cout << "+";    // in use
            nfr++;
        }
        else{
            cout << "-";    // empty
            fr++;
        }
    }
    cout << "\n";
    cout << "Free blocks count       : " << fr << "\n"
         << "Non-Free block count    : " << nfr << endl;
    //free(bitmap);
    // read root inode
    cout << "block size: " << block_size << "\n\n";
    inode = (struct ext2_inode *)(addr + BLOCK_OFFSET(group->bg_inode_table) + sizeof(struct ext2_inode));
    cout << "inode table: " << group->bg_inode_table << " block offset: " << BLOCK_OFFSET(group->bg_inode_table) << " size of inode: " << sizeof(struct ext2_inode) << endl;
    //lseek(fd, BLOCK_OFFSET(group -> bg_inode_table)+sizeof(struct ext2_inode), SEEK_SET);
    //read(fd, &inode, sizeof(struct ext2_inode));
    cout << "Reading root inode\n"
            "Size     : "
         << inode->i_size << " bytes\n"
                             "Blocks   : "
         << inode->i_blocks << "\n"; // in number of sectors. A disk sector is 512 bytes.
    */for (int i = 0; i < 15; i++)
    {
        if (i < 12) // direct blocks
            cout << "Block " << i << " : " << inode->i_block[i] << endl;
        else if (i == 12) // single indirect block
            cout << "Single   : " << inode->i_block[i] << endl;
        else if (i == 13) // double indirect block
            cout << "Double   : " << inode->i_block[i] << endl;
        else if (i == 14) // triple indirect block
            cout << "Triple   : " << inode->i_block[i] << endl;
    }

    close(fd);
    close(fd2);
    munmap(addr, sb.st_size);
    return 0;
}
