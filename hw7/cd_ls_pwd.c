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

change_dir()
{
  printf("chage_dir(): to be constructed\n");
}


int list_file()
{
  printf("list_file(): under construction\n");
}


int pwd(MINODE *wd)
{
  printf("pwd(): yet to be done by YOU\n");
}



