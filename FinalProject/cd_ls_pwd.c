/************* cd_ls_pwd.c file **************/

/**** globals defined in main.c file ****/
extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern char   gpath[256];
extern char   *name[64];
extern int    n;
extern int fd, dev;
extern int nblocks, ninodes, bmap, imap, inode_start;
extern char line[256], cmd[32], pathname[256];
extern int isDev;

#define OWNER  000700
#define GROUP  000070
#define OTHER  000007

int change_dir()
{
    printf("cd");
  char temp[EXT2_NAME_LEN];
  MINODE *newip;
  int ino, dev;

  if (pathname[0] == 0)
  {
      iput(running->cwd);
      running->cwd = iget(dev, 2);
      return -1;
  }

  if (pathname[0] == '/')
      dev = root->dev;
  else
      dev = running->cwd->dev;
  strcpy(temp, pathname);
  printf("temp: %s", temp);
  ino = getino(temp);
  if (!ino)
  {
      printf("no such directory\n");
      return -1;
  }
  printf("dev=%d ino=%d\n", dev, ino);
  newip = iget(dev, ino);
  printf("mode=%4x\n", newip->INODE.i_mode);
  if (!S_ISDIR(newip->INODE.i_mode))
  {
      printf("%s is not a directory\n", pathname);
      iput(newip);
      return -1;
  }

  iput(running->cwd);
  running->cwd = newip;
  printf("after cd : cwd = [%d %d]\n", running->cwd->dev, running->cwd->ino);
  return 0;


}

int find_ino(MINODE *mip, u32 *myino)
{
    char buf[BLKSIZE], *cp;
    DIR *dp;

    get_block(mip->dev, mip->INODE.i_block[0], buf);
    cp = buf;
    dp = (DIR *)buf;
    *myino = dp->inode;
    cp += dp->rec_len;
    dp = (DIR *)cp;
    return dp->inode;
}


int findmyname(MINODE *parent, u32 myino, char *myname)
{
    int i;
    char buf[BLKSIZE], temp[256], *cp;
    DIR *dp;
    MINODE *mip = parent;
    //SEARCH FOR FILENAME
    for (i=0; i < 12; i++)
    {  // assume DIR at most 12 direct blocks

        if (mip->INODE.i_block[i] == 0)
            return -1;
        get_block(dev, mip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while(cp < buf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            printf("%4d %4d %4d %s\n",
                   dp->inode, dp->rec_len, dp->name_len, temp);
            if(dp->inode == myino)
            {
                strncpy(myname, dp->name, dp->name_len);
                myname[dp->name_len] = 0;
                //printf("found %s : ino = %d\n",myname,dp->inode);
                return 0;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return -1;
}

int ls_file(MINODE *mip, char *name)
{
    u16 mode, mask;
    mode = mip->INODE.i_mode;
    if (S_ISDIR(mode))
        putchar('d');
    else if (S_ISLNK(mode))
        putchar('1');
    else
        putchar('-');

    mask = 000400;

    for (int i = 0; i < 3; i++)
    {
        if (mode & mask)
            putchar('r');
        else
            putchar('-');
        mask = mask >> 1;

        if (mode & mask)
            putchar('w');
        else
            putchar('-');
        mask = mask >> 1;

        if (mode & mask)
            putchar('w');
        else
            putchar('-');
        mask = mask >> 1;
    }
    printf("%4d", mip->INODE.i_links_count);
    printf("%4d", mip->INODE.i_uid);
    printf("%4d", mip->INODE.i_gid);
    printf("\t");
    printf("%8d", mip->INODE.i_size);
    char filetime[256];
    time_t time = (time_t)mip->INODE.i_mtime;
    strcpy(filetime, ctime(&time));
    filetime[strlen(filetime)-1] = 0;
    printf("\t%s", filetime);
    printf("\t%s", name);
    if (S_ISLNK(mode))
        printf(" -> %s", (char *)mip->INODE.i_block);
    printf("\n");
}

int ls_dir(MINODE *mip)
{
    char buf[BLKSIZE], temp[256];
    DIR *dp;
    char *cp;
    MINODE *dip;

    for (int i = 0; i < 12; i++)
    {
        if (mip->INODE.i_block[i] == 0)
            return 0;
        get_block(mip->dev, mip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;
        while (cp < buf + BLKSIZE)
        {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            dip = iget(dev, dp->inode);
            ls_file(dip, temp);
            iput(dip);

            cp+= dp->rec_len;
            dp = (DIR *)cp;
        }
    }
}

int list_file()
{
     MINODE *mip;
     u16 mode;
     int dev, ino;
     if (pathname[0] == 0)
     {
         return ls_dir(running->cwd);
     }
     dev = root->dev;
     ino = getino(pathname);
     if (ino == 0)
     {
         printf("file does not exist: %s\n", pathname);
         return -1;
     }
     mip = iget(dev, ino);
     mode = mip->INODE.i_mode;
     if (!S_ISDIR(mode))
         ls_file(mip, basename(name[0]));
     else
         ls_dir(mip);
     iput(mip);

}

void rpwd(MINODE *wd)
{
    char myname[256];
    MINODE *parent;
    u32 myino, parentino;
    if (wd==root) return;
    parentino = find_ino(wd, &myino);
    parent = iget(dev, parentino);

    findmyname(parent, myino, myname);
    rpwd(parent);
    iput(parent);
    printf("/%s", myname);
    return;
}


void pwd(MINODE *wd)
{
    if (wd == root)
        printf("/");
    else
        rpwd(wd);
    printf("\n");
}

void myQuit()
{
    MINODE *mip;
    for (int i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        if (mip->refCount > 0 && mip->dirty)
            iput(mip);
    }
}

int enter_name(MINODE *pip, int myino, char *myname)
{
    int i = 0;
    char buf[BLKSIZE];

    for(i =0; i< 12; i++)
    {
        if (pip->INODE.i_block[i] == 0)
            break;
        get_block(dev, pip->INODE.i_block[i], buf);
        dp=(DIR *)buf;
        char * cp = buf;
        int need_length = 4*( (8 + strlen(myname) + 3)/4 );
        if(isDev){printf("length: %d\n", need_length);}

        get_block(pip->dev, pip->INODE.i_block[i], buf);

        dp= (DIR*)buf;
        cp = buf;
        // step to LAST entry in block: int blk = parent->INODE.i_block[i];

        printf("step to LAST entry in data block %d\n", pip->INODE.i_block[i]);
        while (cp + dp->rec_len < buf + BLKSIZE){

            /****** Technique for printing, compare, etc.******
            c = dp->name[dp->name_len];
            dp->name[dp->name_len] = 0;
            printf("%s ", dp->name);
            dp->name[dp->name_len] = c;
            **************************************************/

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
        // dp NOW points at last entry in block
        printf("last_entry: %s\n", dp->name);
        cp = (char *)dp;
        int IDEAL_LEN = 4*( (8 + dp->name_len + 3)/4 );
        int remain = dp->rec_len - IDEAL_LEN;
        printf("remain: %d\n", remain);
        if (remain >= need_length)
        {
            dp->rec_len = IDEAL_LEN;
            cp += dp->rec_len;
            dp = (DIR*)cp;

            dp->inode = myino;
            dp->rec_len = 1024 - ((u32)cp-(u32) buf);
            printf("rec_len: %d\n", dp->rec_len);
            dp->name_len = strlen(myname);
            dp->file_type = EXT2_FT_DIR;
            strcpy(dp->name, myname);
            put_block(dev,pip->INODE.i_block[i],buf);
            return 1;

        }
    }
    printf("Number of Data Blocks: %d\n", i);

    dp->inode=myino;
    dp->rec_len = BLKSIZE;
    dp->name_len=strlen(myname);
    dp->file_type=EXT2_FT_DIR;
    strcpy(dp->name, myname);

    put_block(dev, pip->INODE.i_block[i], buf);
    return 1;


}


int mymkdir(MINODE *pip, char *name)
{
    int ino = ialloc(dev);
    if (isDev){printf("ino: %d", ino);}
    int bno = balloc(dev);
    if (isDev){printf("bno: %d", bno);}
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
    ip->i_uid  = running->uid;	// Owner uid
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = BLKSIZE;		// Size in bytes
    ip->i_links_count = 2;	        // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 2;                	// LINUX: Blocks count in 512-byte chunks
    ip->i_block[0] = bno;             // new DIR has one data block
    for (int i =0; i < 15; i++) {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);

    char buf[BLKSIZE];
    char *cp;
    get_block(dev, bno, buf);
    dp=(DIR*)buf;
    cp=buf;

    dp->inode=ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->file_type=(u8)EXT2_FT_DIR;
    dp->name[0] ='.';
    cp+=dp->rec_len;
    dp=(DIR*)cp;

    dp->inode= pip->ino;
    dp->rec_len=BLKSIZE-12;
    dp->name_len=2;
    dp->file_type=(u8)EXT2_FT_DIR;
    dp->name[0]=dp->name[1]='.';

    put_block(dev,bno, buf);
    enter_name(pip, ino, name);
//    pip->dirty = 1;
//    iput(pip);
    return 1;



}


int mk_dir()
{
    MINODE *mip;
    int dev;
    if (pathname[0] == '/') {
        mip = root;
    }
    else
    {
        mip = running->cwd;
    }
    dev = mip->dev;

    char temp[BLKSIZE];
    char parent[BLKSIZE];
    char child[BLKSIZE];

    strcpy(temp, pathname);
    strcpy(parent, dirname(temp));

    strcpy(temp, pathname);
    strcpy(child, basename(temp));

    int pino = getino(parent);
    MINODE * pip = iget(dev, pino);

    if(!S_ISDIR(pip->INODE.i_mode))
    {
        printf("error: not a directory\n");
        return 0;
    }
    if (getino(pathname) != 0)
    {
        printf("directory already exists!\n");
        return 0;
    }
    mymkdir(pip, child);
    pip->INODE.i_links_count++;
    pip->INODE.i_atime=time(0L);
    pip->dirty = 1;

    iput(pip);
    return 1;

}


int my_creat(MINODE *pip, char *name)
{
    int ino = ialloc(dev);
    if (isDev){printf("ino: %d", ino);}
    int bno = balloc(dev);
    if (isDev){printf("bno: %d", bno);}
    MINODE *mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    ip->i_mode = 0x81A4;		// OR 0100644: FILE type
    ip->i_uid  = running->uid;	// Owner uid
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = 0;		// Size in bytes
    ip->i_links_count = 1;	        // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);  // set to current time
    ip->i_blocks = 0;                	// LINUX: Blocks count in 512-byte chunks
    ip->i_block[0] = bno;             // new DIR has one data block
    for (int i =0; i < 15; i++) {
        ip->i_block[i] = 0;
    }
    mip->dirty = 1;
    iput(mip);
    enter_name(pip, ino, name);
    return ino;

}


int creat_file()
{
    MINODE *mip;
    int dev;
    if (pathname[0] == '/') {
        mip = root;
    }
    else
    {
        mip = running->cwd;
    }
    dev = mip->dev;

    char temp[BLKSIZE];
    char parent[BLKSIZE];
    char child[BLKSIZE];

    strcpy(temp, pathname);
    strcpy(parent, dirname(temp));

    strcpy(temp, pathname);
    strcpy(child, basename(temp));

    int pino = getino(parent);
    MINODE * pip = iget(dev, pino);

    if(!S_ISDIR(pip->INODE.i_mode))
    {
        printf("error: not a directory\n");
        return 0;
    }
    if (getino(pathname) != 0)
    {
        printf("file already exists!\n");
        return 0;
    }
    my_creat(pip, child);

    pip->INODE.i_atime=time(0L);
    pip->dirty = 1;

    iput(pip);
    return 1;
}
