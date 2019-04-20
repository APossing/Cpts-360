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

void decFreeBlocks(int dev)
{
    char buf[BLKSIZE];
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_blocks_count--;
    put_block(dev, 1,buf);

    get_block(dev, 2,buf);
    gp = (GD *)buf;
    gp->bg_free_blocks_count--;
    put_block(dev, 2,buf);
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
int tokenize(char *pathname)
{
    char *s;
    strcpy(line, pathname);
    n = 0;
    s = strtok(line, "/");
    while(s){
        name[n++] = s;
        s = strtok(0, "/");
    }
}
int search(MINODE *mip, char *name)
{
    int i;
    char *cp, temp[256], sbuf[BLKSIZE];
    DIR *dp;
    for (i=0; i<12; i++){ // search DIR direct blocks only
        if (mip->INODE.i_block[i] == 0)
            return 0;
        get_block(mip->dev, mip->INODE.i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;
        while (cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%8d%8d%8u %s\n",
                   dp->inode, dp->rec_len, dp->name_len, temp);
            if (strcmp(name, temp)==0){
                printf("found %s : inumber = %d\n", name, dp->inode);
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

int tst_bit(char *buf, int BIT)
{
    return buf[BIT / 8] & (1 << (BIT % 8));
}

int set_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] |= (1 << j);
}

int clr_bit(char *buf, int bit)
{
    int i, j;
    i = bit/8; j=bit%8;
    buf[i] &= ~(1 << j);
}

int ialloc(int dev)
{
    int  i;
    char buf[BLKSIZE];

    // read inode_bitmap block
    get_block(dev, imap, buf);

    for (i=0; i < ninodes; i++){
        if (tst_bit(buf, i)==0){
            set_bit(buf,i);
            put_block(dev, imap, buf);
            return i+1;
        }
    }
    return 0;
}


unsigned long balloc(int dev)
{
    int i;
    char buf[BLKSIZE];
    int nblocks;
    SUPER *temp;

    // get total number of blocks
    get_block(dev,1,buf);
    temp = (SUPER *)buf;
    nblocks = temp->s_blocks_count;
    put_block(dev,1,buf);

    get_block(dev, bmap,buf);

    for(i = 0; i < nblocks ; i++)
    {
        if(tst_bit(buf,i) == 0)
        {
            set_bit(buf,i);
            put_block(dev,bmap,buf);

            decFreeBlocks(dev);
            return i+1;
        }
    }
    return 0;
}
int incFreeInodes(int dev)
{
    char buf[BLKSIZE];
// inc free inodes count in SUPER and GD
    get_block(dev, 1, buf);
    sp = (SUPER *)buf;
    sp->s_free_inodes_count++;
    put_block(dev, 1, buf);
    get_block(dev, 2, buf);
    gp = (GD *)buf;
    gp->bg_free_inodes_count++;
    put_block(dev, 2, buf);
}

int idalloc(int dev, int ino)
{
    int i;
    char buf[BLKSIZE];
// get inode bitmap block
    get_block(dev, imap, buf);
    clr_bit(buf, ino-1);
// write buf back
    put_block(dev, imap, buf);
// update free inode count in SUPER and GD
    incFreeInodes(dev);
}

void bdalloc(int dev, int block)
{
    char buff[BLKSIZE];
    get_block(dev, bmap, buff);
    clr_bit(buff, block-1);
    put_block(dev, bmap, buff);
}

void rm_child(MINODE *pmip, char *name)
{
    INODE* pip = &pmip->INODE;
    char sbuf[BLKSIZE], temp[256];
    DIR *dp, *startDp;
    char  *finalCp, *cp;
    int first, last;
    DIR *predDir;

    for (int i=0; i < 12; i++)
    {  // assume DIR at most 12 direct blocks

        if (pip->i_block[i] == 0)
            return;
        printf("i=%d i_block[%d]=%d\n",i,i,pip->i_block[i]);
        get_block(dev, pip->i_block[i], sbuf);

        dp = (DIR *)sbuf;
        cp = sbuf;
        int total_length = 0;
        while(cp < sbuf + BLKSIZE){
            strncpy(temp, dp->name, dp->name_len);
            total_length += dp->rec_len;
            temp[dp->name_len] = 0;
            if (!strcmp(temp, name))
            {
                if (cp == sbuf && cp + dp->rec_len == sbuf + BLKSIZE) //first
                {
                    memset(sbuf, '\0', BLKSIZE);
                    bdalloc(dev, ip->i_block[i]);

                    pip->i_size -=BLKSIZE;
                    while(pip->i_block[i+i] && i+1 < 12)
                    {
                        get_block(dev, pip->i_block[i], sbuf);
                        put_block(dev, pip->i_block[i-1], sbuf);
                        i++;
                    }
                }
                else if(cp+dp->rec_len == sbuf + BLKSIZE) //last
                {
                    predDir->rec_len +=dp->rec_len;
                    put_block(dev, pip->i_block[i], sbuf);
                }
                else //middle
                {
                    int removed_length = dp->rec_len;
                    char* cNext = cp + dp->rec_len;
                    DIR* dNext = (DIR *)cNext;
                    while(total_length + dNext->rec_len < BLKSIZE)
                    {
                        total_length += dNext->rec_len;
                        int next_length = dNext->rec_len;
                        dp->inode = dNext->inode;
                        dp->rec_len = dNext->rec_len;
                        dp->name_len = dNext->name_len;
                        strncpy(dp->name, dNext->name, dNext->name_len);
                        cNext += next_length;
                        dNext = (DIR *)cNext;
                        cp+= next_length;
                        dp = (DIR *)cp;
                    }
                    dp->inode = dNext->inode;
                    // add removed rec_len to the last entry of the block
                    dp->rec_len = dNext->rec_len + removed_length;
                    dp->name_len = dNext->name_len;
                    strncpy(dp->name, dNext->name, dNext->name_len);
                    put_block(dev, pip->i_block[i], sbuf); // save
                    pmip->dirty = 1;
                    return;
                }
                pmip->dirty=1;
                iput(pmip);
                return;
            }
            predDir = dp;
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}

void deallocateInodeDataBlocks(MINODE* mip)
{
    char bitmap[1024],dblindbuff[1024], buff[BLKSIZE];
    int i = 0;
    int j = 0;
    int indblk,dblindblk;
    unsigned long *indirect,*doubleindirect;
    get_block(dev,bmap,bitmap);
    for ( i = 0; i<12; i++)
    {
        if (mip->INODE.i_block[i]!=0)
        {
            clr_bit(bitmap, mip->INODE.i_block[i]-1);
            mip->INODE.i_block[i]=0;
        }
        else
        {
            put_block(dev,bmap,bitmap);
            return ;
        }
    }
    if (mip->INODE.i_block[i]!=0)
    {
        indblk = mip->INODE.i_block[i];
        get_block(dev,indblk,buff);
        indirect = (unsigned long *)buff;
        for (i=0;i<256;i++)
        {
            if(*indirect != 0)
            {
                clr_bit(bitmap, *indirect-1);
                *indirect = 0;
                indirect++;
            }
            else
            {
                clr_bit(bitmap, indblk-1);
                put_block(dev,indblk,buff);
                put_block(dev,bmap,bitmap);
                mip->INODE.i_block[12] = 0;
                return;
            }
        }
    }
    else
    {
        put_block(dev,bmap,bitmap);
        return;
    }
    if (mip->INODE.i_block[13]!=0)
    {
        dblindblk = mip->INODE.i_block[13];
        get_block(dev,dblindblk,dblindbuff);
        doubleindirect = (unsigned long *)dblindbuff;
        for (i=0;i<256;i++)
        {
            indblk = *doubleindirect;
            get_block(dev,indblk,buff);
            indirect = (unsigned long *)buff;
            for (j=0;j<256;j++)
            {
                if(*indirect != 0)
                {
                    clr_bit(bitmap, *indirect-1);
                    *indirect = 0;
                    indirect++;
                }
                else
                {
                    clr_bit(bitmap, indblk-1);
                    clr_bit(bitmap, dblindblk-1);
                    put_block(dev,indblk,buff);
                    put_block(dev,bmap,bitmap);
                    put_block(dev,dblindblk,dblindbuff);
                    mip->INODE.i_block[13] = 0;
                    return;
                }
                clr_bit(bitmap, indblk-1);

            }
            doubleindirect++;
            if (*doubleindirect == 0)
            {
                clr_bit(bitmap, indblk-1);
                clr_bit(bitmap, dblindblk-1);
                put_block(dev,indblk,buff);
                put_block(dev,bmap,bitmap);
                put_block(dev,dblindblk,dblindbuff);
                mip->INODE.i_block[13] = 0;
                return;
            }
        }
    }
    else
    {
        put_block(dev,bmap,bitmap);
        return;
    }

}

void my_truncate(MINODE *mip)
{
    deallocateInodeDataBlocks(mip);
    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    mip->INODE.i_size = 0;
    mip->dirty = 1;
}

