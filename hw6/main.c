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
    struct ext2_super_block * sp = (struct ext2_super_block *)buf;
    if (0xEF53 != sp->s_magic)
    {
        printf("Error: Not an EXT2 file sytem\n");
        return -1;
    }
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
            // YOU SHOULD print i_block[i] number here
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
    char sbuf[BLKSIZE], temp[256];
    struct ext2_dir_entry_2 *dp;
    char *cp;
    int i;

    for (i=0; i < 12; i++)
    {  // assume DIR at most 12 direct blocks
        if (ip->i_block[i] == 0)
            break;
        // YOU SHOULD print i_block[i] number here
        get_block(dev, ip->i_block[i], sbuf);

        dp = (struct ext2_dir_entry_2 *)sbuf;
        cp = sbuf;

        while(cp < sbuf + BLKSIZE){
            if(strcmp(dp->name, name) == 0)
                return ip->i_block[i];
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
    printf("copy: %s\n", copy);
    outputArr[0] = strtok(copy,"/");
    int i = 1;
    for(i = 1; outputArr[i] = strtok(NULL,"/");i++)
    {
        printf("output %d: %s\n",i,outputArr[i]);
    }
    return i;
}

int main() {
    int dev = loadFilesystem("diskimage");
    char buf[BLKSIZE];
    char buf2[BLKSIZE];
    getSuper(dev, buf);
    if (is_ext2(buf))
    {
        char buf2[BLKSIZE];
        getGd(dev, buf2);
        struct ext2_group_desc *gd = (struct ext2_group_desc*)buf;
        printf("bitmap: %d\n", gd->bg_block_bitmap);
        printf("imap: %d\n", gd->bg_inode_bitmap);
        printf("inodes_start: %d\n", gd->bg_inode_table);
        get_block(dev, gd->bg_inode_table, ibuf);
        struct ext2_inode *ip = (struct ext2_inode *)ibuf + 1;
        show_dir(ip, dev);
        //search(ip, "", dev);
        char *output[1024];
        int count = tokenizePathName("/cs360/is/fun", output);
        printf("count: %d\n", count);
        for (int i = 0; i < count; i++)
        {
            printf("%s\n", output[i]);
        }
    }
    return 0;
}