/*********** util.c file ****************/

#include <unistd.h>

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int    fd, dev;
extern int    nblocks, ninodes, bmap, imap, inode_start;
extern char   line[256], cmd[32], pathname[256];

int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

int tokenize(char *pathname)
{
    name[0] = 0;
    name[0] = strtok(pathname, "/");
    for (n = 1; name[n] = strtok(NULL, "/"); n++)
    {
        printf("n: %s", n, name[n]);
    }
  // tokenize pathname in GLOBAL gpath[]
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
      //printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
       mip->refCount = 1;
       mip->dev = dev;
       mip->ino = ino;

       // get INODE of ino to buf    
       blk  = (ino-1) / 8 + inode_start;
       disp = (ino-1) % 8;

       //printf("iget: ino=%d blk=%d disp=%d\n", ino, blk, disp);

       get_block(dev, blk, buf);
       ip = (INODE *)buf + disp;
       // copy INODE to mp->INODE
       mip->INODE = *ip;

       return mip;
    }
  }   
  printf("PANIC: no more free minodes\n");
  return 0;
}

void iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return;
 if (!mip->dirty)       return;
 
 /* write back */
 printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

int mySearchCompare(char str1[], char str2[], int len)
{
    int i = 0;
    for(i = 0; i < len && str2[i] != 0; i++)
    {
        if (str1[i] > str2[i])
            return 1;
        if (str1[i] < str2[i])
            return -1;
    }
    if (str2[i] != 0)
        return -1;
    return 0;
}

int search(MINODE *mip, char *name)
{
  printf("searching for %s in %d\n", name, dev);
    char sbuf[BLKSIZE], temp[256];
    DIR *dp;
    char *cp;
    int i;
    ip = &(mip->INODE);

    for (i=0; i < 12; i++)
    {  // assume DIR at most 12 direct blocks

        if (ip->i_block[i] == 0)
            break;
        printf("i=%d i_block[%d]=%d\n",i,i,ip->i_block[i]);
        get_block(dev, ip->i_block[i], sbuf);

        dp = (DIR *)sbuf;
        cp = sbuf;

        while(cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%4d %4d %4d %s\n",
                   dp->inode, dp->rec_len, dp->name_len, temp);
            printf("%s, %s compare\n",dp->name, name);
            if(mySearchCompare(dp->name, name, dp->name_len) == 0)
            {
                printf("found %s : ino = %d\n",name,dp->inode);
                return dp->inode;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return 0;
}

int getino(char *pathname)
{
  int i, ino, blk, disp;
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);
  if (strcmp(pathname, "/")==0)
      return 2;

  if (pathname[0]=='/')
    mip = iget(dev, 2);//TODO maybe *dev
  else
    mip = iget(running->cwd->dev, running->cwd->ino);

  tokenize(pathname);

  for (i=0; i<n; i++){
      printf("===========================================\n");
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);
      mip = iget(dev, ino);
   }
   return ino;
}

