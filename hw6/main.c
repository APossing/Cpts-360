#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ext2fs/ext2_fs.h>
#define BLKSIZE 1024
#define SUPERBLOCK 1
#define GROUPDESCRIPTOR 2
char ibuf[BLKSIZE];
/*
struct ext2_group_desc
{
    __u32	bg_block_bitmap;	 Blocks bitmap block
    __u32	bg_inode_bitmap;	 Inodes bitmap block
    __u32	bg_inode_table;		 Inodes table block
    __u16	bg_free_blocks_count;	 Free blocks count
    __u16	bg_free_inodes_count;	 Free inodes count
    __u16	bg_used_dirs_count;	 Directories count
    __u16	bg_pad;
    __u32	bg_reserved[3];
};
*/

int loadFilesystem(char* filesystem_file)
{
    int fd = open(filesystem_file, O_RDONLY);
    if (fd < 0)
    {
        printf("open failed!\n");
        exit(1);
    }
    return fd;
}

int is_ext2(char *buf)
{
    printf("...Checking if it is a ext2 FS...\n");
    struct ext2_super_block * sp = (struct ext2_super_block *)buf;
    if (0xEF53 != sp->s_magic)
    {
        printf("Error: Not an EXT2 file sytem\n");
        return -1;
    }
    printf("It is!\n");
    return 1;
}

// get_block() reads a disk BLOCK into a char buf[BLKSIZE].
int get_block(int dev, int blk, char *buf)
{
    lseek(dev, blk*BLKSIZE, SEEK_SET);
    return read(dev, buf, BLKSIZE);
}

int getSuper(int dev1, char *buf)
{
    get_block(dev1, SUPERBLOCK, buf);
}

int getGd(int dev1, char *buf)
{
    get_block(dev1, GROUPDESCRIPTOR, buf);
}


void show_dir(struct ext2_inode *ip, int dev)
{
    char sbuf[BLKSIZE], temp[256];
    struct ext2_dir_entry_2 *dp;
    char *cp;
    int i;
    for (i=0; i < 12; i++)
        {  // assume DIR at most 12 direct blocks
            if (ip->i_block[i] == 0)
            break;
            printf("Root inode data block = %d\n\n", ip->i_block[0]);
            get_block(dev, ip->i_block[i], sbuf);

            dp = (struct ext2_dir_entry_2 *)sbuf;
            cp = sbuf;

            while(cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%4d %4d %4d %s\n",
            dp->inode, dp->rec_len, dp->name_len, temp);

            cp += dp->rec_len;
            dp = (struct ext2_dir_entry_2 *)cp;
        }
    }
}


int search(struct ext2_inode *ip, char *name, int dev)
{
    printf("searching for %s in %d\n", name, dev);
    char sbuf[BLKSIZE], temp[256];
    struct ext2_dir_entry_2 *dp;
    char *cp;
    int i;

    for (i=0; i < 12; i++)
    {  // assume DIR at most 12 direct blocks

        if (ip->i_block[i] == 0)
            break;
        printf("i=%d i_block[%d]=%d\n",i,i,ip->i_block[i]);
        get_block(dev, ip->i_block[i], sbuf);

        dp = (struct ext2_dir_entry_2 *)sbuf;
        cp = sbuf;

        while(cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%4d %4d %4d %s\n",
                   dp->inode, dp->rec_len, dp->name_len, temp);
            if(strcmp(dp->name, name) == 0)
            {
                printf("found %s : ino = %d\n",name,dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (struct ext2_dir_entry_2 *)cp;
        }
    }
    return 0;
}

int tokenizePathName(char* pathname, char* outputArr[])
{
    char copy[2048];
    strcpy(copy,pathname);
    printf("\ntokenize %s\n", copy);
    outputArr[0] = strtok(copy,"/");
    int i = 1;
    for(i = 1; outputArr[i] = strtok(NULL,"/");i++)
    {
        printf("output %d: %s\n",i,outputArr[i]);
    }

    for (int j = 0; j < i; j++)
    {
        printf("%s ", outputArr[j]);
    }
    printf("\n\n");
    return i;
}


void printIndirect(int dev,int block, int offset)
{
    char buf[BLKSIZE];
    get_block(dev,block,buf);
    struct ext2_inode *ipIndirect = (struct ext2_inode *)buf;
    for (int i = 0-offset; ipIndirect->i_block[i] != 0 && i < 256-offset; i++)
    {
        printf("%d\t", ipIndirect->i_block[i]);
    }
}

void printDoubleIndirect(int dev,int block, int offset)
{
    char buf[BLKSIZE];
    get_block(dev,block,buf);
    struct ext2_inode *ipIndirect = (struct ext2_inode *)buf;
    for (int i = 0-offset; ipIndirect->i_block[i] != 0 && i < 256-offset; i++)
    {
        printIndirect(dev, ipIndirect->i_block[i], offset);
    }
}
void printTripleIndirect(int dev,int block, int offset)
{
    char buf[BLKSIZE];
    get_block(dev,block,buf);
    struct ext2_inode *ipIndirect = (struct ext2_inode *)buf;

    for (int i = 0-offset; ipIndirect->i_block[i] != 0 && i < 256-offset; i++)
    {
        printDoubleIndirect(dev, ipIndirect->i_block[i], offset);
    }
}

void printAllBlocks(int dev, const struct ext2_inode node, int offset)
{
    for (int i=0; i < 15; i++)
        printf("iblock[%d]: %d\n", i, node.i_block[i]);

    if (node.i_block[12] != 0) {
        printf("\n----------- INDIRECT BLOCKS ---------------\n");
        printIndirect(dev, node.i_block[12], offset);
    }

    if (node.i_block[13] != 0) {
        printf("\n----------- DOUBLE INDIRECT BLOCKS ---------------\n");
        printDoubleIndirect(dev, node.i_block[13], offset);
    }

    if (node.i_block[14] != 0) {
        printf("\n----------- TRIPLE INDIRECT BLOCKS ---------------\n");
        printTripleIndirect(dev, node.i_block[14], offset);
    }
}

int main() {
    int dev = loadFilesystem("diskimage");
    char buf[BLKSIZE];
    getSuper(dev, buf);
    if (is_ext2(buf))
    {
        getGd(dev, buf);
        struct ext2_group_desc *gd = (struct ext2_group_desc*)buf;
        printf("bitmap: %d\n", gd->bg_block_bitmap);
        printf("imap: %d\n", gd->bg_inode_bitmap);
        printf("inodes_start: %d\n", gd->bg_inode_table);
        get_block(dev, gd->bg_inode_table, ibuf);
        struct ext2_inode *ip = (struct ext2_inode *)ibuf + 1;
        show_dir(ip, dev);
        char *output[1024];
        int count = tokenizePathName("/Z/hugefile", output);

        int ino, blk, offset;
        int i;
        for (i=0; i < count; i++){
            ino = search(ip, output[i], dev);
            if (ino==0)
            {
                printf("can't find %s\n", output[i]);
                exit(1);
            }

            // Mailman's algorithm: Convert (dev, ino) to INODE pointer
            blk    = (ino - 1) / 8 + gd->bg_inode_table;
            offset = (ino - 1) % 8;
            get_block(dev, blk, ibuf);
            ip = (struct ext2_inode *)ibuf + offset;   // ip -> new INODE
        }
        ////////////////////////////////////////////////////////////////////////
        printAllBlocks(dev, *ip, gd->bg_inode_table);
        ////////////////////////////////////////////////////////////////////////



    }
    return 0;
}