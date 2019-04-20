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
    char *t1 = "xwrxwrxwr-------";
    char *t2 = "----------------";
    u16 mode, mask;
    mode = mip->INODE.i_mode;
    if ((mode & 0xF000) == 0x8000) // if (S_ISREG())
        printf("%c",'-');
    if ((mode & 0xF000) == 0x4000) // if (S_ISDIR())
        printf("%c",'d');
    if ((mode & 0xF000) == 0xA000) // if (S_ISLNK())
        printf("%c",'l');
    for (int i=8; i >= 0; i--){
        if (mode & (1 << i)) // print r|w|x
            printf("%c", t1[i]);
        else
            printf("%c", t2[i]);
// or print -
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
        printf("block: %d\n", mip->INODE.i_block[i]);
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
    int i;
    MINODE *mip;
    for (i=0; i<NMINODE; i++){
        mip = &minode[i];
        if (mip->refCount > 0)
            iput(mip);
    }
    exit(0);
}

int isEmpty(MINODE *mip)
{
    int i;
    char buf[BLKSIZE], namebuf[256], *cp;

    // more than 2 links?
    if(mip->INODE.i_links_count > 2)
        return 0;

    // only 2 links?
    if(mip->INODE.i_links_count == 2)
    {
        // cycle through each direct block to check...
        for(i = 0; i <= 11; i++)
        {
            if(mip->INODE.i_block[i])
            {
                get_block(mip->dev, mip->INODE.i_block[i], buf);
                cp = buf;
                dp = (DIR *)buf;

                while(cp < &buf[BLKSIZE])
                {
                    strncpy(namebuf, dp->name, dp->name_len);
                    namebuf[dp->name_len] = 0;

                    // if stuff exists, this directory isn't empty :(
                    if(strcmp(namebuf, ".") && strcmp(namebuf, ".."))
                        return 0;

                    cp+=dp->rec_len;
                    dp=(DIR *)cp;
                }
            }
        }
        return 1;
    }
    return -1;
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
    for (int i =1; i < 16; i++) {
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
    pip->dirty = 1;
    iput(pip);
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
    pip->INODE.i_atime = pip->INODE.i_ctime = pip->INODE.i_mtime = time(0L);
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
    for (int i =1; i < 16; i++) {
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

int my_link()
{
    char * oldFile = strtok(pathname, " ");
    char * newLink = strtok(NULL, " ");

    int oldIno = getino(oldFile);

    if (!oldIno)
    {
        printf("Old File %s Does not exist!", oldFile);
        return -1;
    }
    MINODE *oldMip = iget(dev, oldIno);

    if (S_ISDIR(oldMip->INODE.i_mode)) {
        printf("%s is a directoy", oldFile);
        return -1;
    }

    int new_ino = getino(newLink);
    if (new_ino)
    {
        printf("Link %s Already exists!", newLink);
        return -1;
    }


    char temp[BLKSIZE];

    strcpy(temp, newLink);
    char* parent = dirname(temp);

    strcpy(temp, newLink);
    char *child = basename(temp);

    int pino = getino(parent);

    MINODE * pmip = iget(dev, pino);

    enter_name(pmip, oldMip->ino, child);

    oldMip->INODE.i_links_count++;
    oldMip->dirty = 1;
    printf("link count for %s is now %d\n", oldFile, oldMip->INODE.i_links_count);

    iput(oldMip);
    iput(pmip);
    return 1;

}

int my_symlink(){
    char * oldpath = strtok(pathname, " ");
    char * newpath = strtok(NULL, " ");

    char parentdir[64], name[64], *cp;
    DIR *dp;
    MINODE *pip, *targetip;
    int parent;
    cp = strrchr(newpath, '/');
    if(cp == NULL){
        parent = running ->cwd->ino;
        strcpy(name, newpath);
    }else{
        *(cp) = '\0';
        strcpy(parentdir, newpath);
        parent = getino(parentdir);
        strcpy(name, cp+1);
    }
    strcpy(pathname, newpath);
    int target = creat_file();
    pip = iget(fd, parent);
    targetip = iget(fd, target);
    pip->dirty = 1;
    pip->INODE.i_links_count++;

    iput(pip);

    targetip->INODE.i_mode = 0xA1A4;
    targetip->INODE.i_size = strlen(oldpath);
    memcpy(targetip->INODE.i_block, oldpath, strlen(oldpath));
    iput(targetip);

    return 0;
}


int my_unlink()
{

    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);

    if((mip->INODE.i_mode & 0100000) != 0100000){
        printf("Error: Cannot unlink NON-REG files\n");
        return -1;
    }
    char temp[1024];
    strcpy(temp,pathname);
    char * parent = dirname(temp);
    strcpy(temp,pathname);
    char * child = basename(temp);
    int pino = getino(parent);
    MINODE * pmip = iget(dev, pino);
    pmip->dirty = 1;
    iput(pmip);
    mip->INODE.i_links_count--;
    if (mip->INODE.i_links_count > 0)
    {
        mip->dirty = 1;
    }
    else
    {
        if (!((mip->INODE.i_mode) & 0xA1FF) == 0xA1FF) {
            deallocateInodeDataBlocks(mip);
        }
        mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
        mip->INODE.i_size = 0;
        mip->dirty = 1;
        idalloc(dev, mip->ino);
    }
    iput(mip);
    rm_child(pmip, child);
}


int my_stat()
{
    struct stat myStat2;
    struct stat *myStat = &myStat2;
    int ino = getino(pathname);
    MINODE *mip = iget(fd, ino);
    myStat->st_dev = fd;
    memcpy(&myStat->st_ino, &ino, sizeof(ino_t));
    memcpy(&myStat->st_mode, &mip->INODE.i_mode, sizeof(mode_t));
    memcpy(&myStat->st_nlink, &mip->INODE.i_links_count, sizeof(nlink_t));
    memcpy(&myStat->st_uid, &mip->INODE.i_uid, sizeof(uid_t));
    memcpy(&myStat->st_gid, &mip->INODE.i_gid, sizeof(gid_t));
    memcpy(&myStat->st_size, &mip->INODE.i_size, sizeof(off_t));
    myStat->st_blksize = 1024;
    memcpy(&myStat->st_blocks, &mip->INODE.i_blocks, sizeof(blkcnt_t));
    memcpy(&myStat->st_atime, &mip->INODE.i_atime, sizeof(time_t));
    memcpy(&myStat->st_mtime, &mip->INODE.i_mtime, sizeof(time_t));
    memcpy(&myStat->st_ctime, &mip->INODE.i_ctime, sizeof(time_t));
    printf("dev: %d\t", (int)myStat->st_dev);
    printf("ino: %u\t\t", (int)myStat->st_ino);
    printf("mode: %u\t", (unsigned short)myStat->st_mode);
    printf("nlink: %lu\t", (unsigned long)myStat->st_nlink);
    printf("uid: %u\t", (int)myStat->st_uid);
    printf("\n");
    printf("gid: %u\t", (int)myStat->st_gid);
    printf("size: %d\t", (int)myStat->st_size);
    printf("blksize: %d\t", (int)myStat->st_blksize);
    printf("blocks: %lu\t", (unsigned long)myStat->st_blocks);
    char *time_string = ctime(&myStat->st_ctime);
    printf("\nctime: %s", time_string);
    time_string = ctime(&myStat->st_atime);
    printf("atime: %s", time_string);
    time_string = ctime(&myStat->st_mtime);
    printf("mtime: %s", time_string);
    printf("\n");

    iput(mip);
    return 0;
}

int my_touch()
{
    int ino = getino(pathname);
    if (ino == 0)
    {
        creat_file();
    }
    ino = getino(pathname);
    MINODE *mip = iget(fd, ino);


    mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
    iput(mip);
}

int my_chmod()
{
    char * filename = strtok(pathname, " ");
    char * modeStr = strtok(NULL, " ");
    int mode = atoi(modeStr);
    int ino;
    MINODE *mip;
    ino = getino(filename);

    if(!ino)
        return 0;
    mip = iget(dev, ino);
    printf("previous Imode: %d", mip->INODE.i_mode);
    // change its permissions accordingly to those the user desires
    mip->INODE.i_mode = mode;
    printf("new Imode: %d", mip->INODE.i_mode);

    // mark dirty
    mip->dirty = 1;

    iput(mip); // cleanup
    return 0;
}

void my_rmdir()
{
    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    if (!S_ISDIR(mip->INODE.i_mode)) {
        printf("%s is not a directory", pathname);
        return;
    }
    if (mip->refCount > 2)
    {
        printf("ref count is %d so it is not available\n", mip->refCount);
        return;
    }
    if(!isEmpty(mip))
    {
        printf("DIR not very empty\n");
        iput(mip);
        return;
    }

    for (int i=0; i<12; i++){
        if (mip->INODE.i_block[i]==0)
            continue;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }
    idalloc(mip->dev, mip->ino);
    iput(mip); //(which clears mip->refCount = 0);

    int pino = find_ino(mip, &ino);
    MINODE *pmip = iget(mip->dev, pino);

    //findmyname(pmip, ino, pathname);
    rm_child(pmip,pathname);
    pmip->INODE.i_links_count--;
    pmip->dirty = 1;
    iput(pmip);

}

int my_open()
{
    char * filename = strtok(pathname, " ");
    char * modeStr = strtok(NULL, " ");

    int mode;
    if (strcmp(modeStr,"R") == 0)
    {
        mode = 0;
    }
    else if (strcmp(modeStr,"W") == 0)
    {
        mode = 1;
    }
    else if (strcmp(modeStr,"RW") == 0)
    {
        mode = 2;
    }
    else if (strcmp(modeStr,"APPEND") == 0)
    {
        mode = 3;
    }
    else
    {
        printf("open: not proper mode: %s\n", modeStr);
    }

    int ino = getino(filename);
    if (!ino)
    {
        strcpy(pathname, filename);
        creat_file();
        ino = getino(filename);
    }
    MINODE* mip = iget(dev, ino);

    if((mip->INODE.i_mode & 0100000) != 0100000){
        printf("Error: Cannot open non-regular files\n");
        return -1;
    }
    int lowFd;
    for(int i = 0; i <NFD; i++)
    {
        if (running->fd[i] == NULL)
        {
            lowFd = i;
            break;
        }

        if (running->fd[i]->mptr == mip)
        {
            if (mode > 0)
            {
                printf("open with incompatable mode: %s\n",filename);
                return -1;
            }
        }
    }
    OFT *oftp;
    oftp->mode = mode;      // mode = 0|1|2|3 for R|W|RW|APPEND
    oftp->refCount = 1;
    oftp->mptr = mip;  // point at the file's minode[]

    switch(mode){
        case 0 : oftp->offset = 0;     // R: offset = 0
            break;
        case 1 : my_truncate(mip);        // W: truncate file to 0 size
            oftp->offset = 0;
            break;
        case 2 : oftp->offset = 0;     // RW: do NOT truncate file
            break;
        case 3 : oftp->offset =  mip->INODE.i_size;  // APPEND mode
            break;
        default: printf("invalid mode\n");
            return -1;
    }
    running->fd[lowFd] = oftp;
    mip->INODE.i_atime = time(NULL);
    mip->dirty = 1;
    iput(mip);

    return lowFd;
}

int close_file(int fd)
{
    if(fd < 0)
    {
        printf("fd is invalid: %d",fd);
        return -1;
    }
    OFT *oftp = running->fd[fd];
    running->fd[fd] = NULL;
    oftp->refCount--;
    if(oftp->refCount>0) return 0;
    MINODE* mip = oftp->mptr;
    iput(mip);
    return 0;
}


int myread(int fd, char buf[ ], int nbytes)
{
    OFT* oftp = running->fd[fd];
    MINODE* mip = oftp->mptr;
    int filesize = mip->INODE.i_size;
    int count = 0;
    char kbuf[BLKSIZE], dbuf[BLKSIZE];
    // number of bytes read
    int offset = oftp->offset;
    // byte offset in file to READ
    //compute bytes available in file: avil = fileSize – offset;
    int avil = filesize- offset;
    while (nbytes && avil){
        int lblk = offset/BLKSIZE;
        int blk;
        int start = offset % BLKSIZE;
        if (lblk < 12) //ezzzzz
        {
            blk = mip->INODE.i_block[lblk];
        }
        else if ((lblk >=12) &&(lblk<256+12)) //indirect
        {
            get_block(mip->dev, mip->INODE.i_block[12], kbuf);
            blk = kbuf[lblk-12];
        }
        else // double indirect
        {
            get_block(mip->dev, mip->INODE.i_block[13], kbuf);
            blk = kbuf[(lblk - (BLKSIZE / sizeof(int)) - 12) % (BLKSIZE / sizeof(int))];
            get_block(mip->dev, blk, dbuf);
            blk = dbuf[(lblk - (BLKSIZE / sizeof(int)) - 12) % (BLKSIZE / sizeof(int))];
        }
        get_block(mip->dev, blk, kbuf);
        char *cp = kbuf + start;
        int remain = BLKSIZE - start;
        while (remain){
            // copy bytes from kbuf[ ] to buf[ ]
            *buf++ = *cp++;
            offset++; count++;
            // inc offset, count;
            remain--; avil--; nbytes--; // dec remain, avail, nbytes;
            if (nbytes==0 || avil==0)
                break;
        } // end of while(remain)
    }
    return count;
}

int read_file()
{
    /*
    Preparations:
    ASSUME: file is opened for RD or RW;
    ask for a fd  and  nbytes to read;
    verify that fd is indeed opened for RD or RW;
    return(myread(fd, buf, nbytes));*/
    char * fdStr = strtok(pathname, " ");
    char * bytesStr = strtok(NULL, " ");
    int fd = atoi(fdStr);
    int bytes = atoi(bytesStr);
    if (running->fd[fd] == NULL)
    {
        return -1;
    }
    if ((running->fd[fd]->mode!=0)&&(running->fd[fd]->mode!=2))
    {
        return -1;
    }
    char buf[BLKSIZE];
    return myread(fd, buf, bytes);





}

int cat()
{
    int i;
    char mybuf[1024], dummy = 0;  // a null char at end of mybuf[ ]
    int n;

    for (i = 0; pathname[i] != 0;i++)
    {

    }
    pathname[i] = ' ';
    pathname[i+1] = 'R';
    pathname[i+2] = 0;
    int fd = my_open();
    printf("=================================\n");
    while( n = myread(fd, mybuf[1024], 1024)){
        mybuf[n] = 0;             // as a null terminated string
        printf("%s", mybuf); //   <=== THIS works but not good
    }
    close_file(fd);
    printf("\n=================================");


}


int my_lseek(int fd, int position) {
    fd = my_open();
    if (running->fd[fd] == NULL) {
        return -1;
    }
    if (position <= running->fd[fd]->mptr->INODE.i_size) {
        int temp = running->fd[fd]->offset;
        running->fd[fd]->offset = position;
        return temp;

    } else {
        return -1;
    }
}

int pfd()
{
    int i;
    printf("Filename\tFD\tmode\toffset\n");
    printf("--------\t--\t----\t------\n");
    for(i = 0;i<NFD;i++)
    {
        if (running->fd[i]!= NULL)
        {
            switch(running->fd[i]->mode)
            {
                case 0:
                    printf("READ\t");
                    break;
                case 1:
                    printf("WRITE\t");
                    break;
                case 2:
                    printf("R/W\t");
                    break;
                case 3:
                    printf("APPEND\t");
                    break;
                default:
                    printf("-------\t");//this should never happen
                    break;
            }
            printf("\t %6d\t %6d\t %5d\n",running->fd[i]->offset, running->fd[i]->mptr->dev, running->fd[i]->mptr->ino);
        }
    }
    return 0;
}

