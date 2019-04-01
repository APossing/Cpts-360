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
    char buf[BLKSIZE], myname[256], *cp;
    MINODE *parent, *ip;
    u32 myino, parentino;
    DIR *dp;
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



