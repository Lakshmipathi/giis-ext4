/*
* /giis/giis-ext4.c-Ext4 Undelete Tool.
*
* Copyright (C) 2010,2011,2012 Lakshmipathi.G <lakshmipathi.g@giis.co.in>
* Visit www.giis.co.in for manuals or docs.
*/

#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <sqlite3.h>
#include <assert.h>    /* assert */
#include <libgen.h>
/* argp */
#include <argp.h>


/* SQL statements used in this program */
#define SQL_STMT_GET_ALL "select name,inode,ext1,blk1,ext2,blk2,ext3,blk3,ext4,blk4,fsize,fpath,mode,owner,gid,md5sum from giistable "
#define SQL_STMT_GET_USR "select name,inode,ext1,blk1,ext2,blk2,ext3,blk3,ext4,blk4,fsize,fpath,mode,owner,gid,md5sum from giistable where owner=?  "
#define SQL_STMT_GET_FTYPE "select name,inode,ext1,blk1,ext2,blk2,ext3,blk3,ext4,blk4,fsize,fpath,mode,owner,gid,md5sum from giistable where name like ? "
#define SQL_STMT_GET_FILE "select name,inode,ext1,blk1,ext2,blk2,ext3,blk3,ext4,blk4,fsize,fpath,mode,owner,gid,md5sum from giistable where name=?  "
#define SQL_STMT_GET_DIRNAMES "select * from giisheader"
#define SQL_STMT_VERIFY_INODE "select * from giistable where inode=?"

#define SQL_STMT_CREATE_HEADER "create table giisheader(max_depth int,update_time int,device_name varchar(100),protected_dir1 varchar(512),protected_dir2 varchar(512),protected_dir3 varchar(512),protected_dir4 varchar(512) NULL,protected_dir5 varchar(512),protected_dir6 varchar(512),protected_dir7 varchar(512));"
#define SQL_STMT_CREATE_TABLE "create table giistable(name varchar(256),inode long ,parent_inode long,mode int,owner int,fflags int,fsize int,ftype varchar(5),fpath varchar(512),gid int,depth int, ext1 int,blk1 long,ext2 int,blk2 long,ext3 int,blk3 long,ext4 int,blk4 long,md5sum varchar(34));"
#define SQL_STMT_INSERT_TABLE "insert into giistable values(?, ?, ?,?, ?, ?,?, ?, ?,?, ?, ?,?, ?, ?,?, ?, ?,?,?)"
#define SQL_STMT_INSERT_HEADER "insert into giisheader values(?,?,?,?,?,?,?,?,?,?)"

	/* dirs */
#define GIISDIR "/usr/local/giis"
#define SQLITE_DB_DIR "/usr/local/giis/db"
#define SQLITE_DB_LOCATION "/usr/local/giis/db/giis-db"
#define GIIS_LOG_FILE "/usr/local/giis/giis.log"
#define RESTORE_DIR "/usr/local/giis/got_it/"


#define handle_error(msg) \
do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define BUF_SIZE 1024
#define TRUE 1
#define FALSE 0



struct linux_dirent {
	long           d_ino;
	off_t          d_off;
	unsigned short d_reclen;
	char           d_name[];
};


struct giis_recovered_file_info{
char *fname; 				/* file name */
char *fpath;				/* original file location */
unsigned long fsize;			/* file size */
int extents[4];				/* No.of extents */
unsigned long  starting_block[4];	/* starting block for extents . Say extent[0] starts at starting_block[0] and extent[1] at starting_block[1] etc */
unsigned long inode_num;
int mode;
int owner;
int group;
char *md5sum;
}s_giis_recovered_file_info,*fi;

struct giis_protected_dir_info{
int max_depth; 
int update_time;
char *device_name;
char *protected_dir[7];
}s_giis_protected_dir_info,*di;


struct arguments
{
   int  flag;		    /* 1-install 2-update 3-recover 4-uninstall*/
};


static struct argp_option options[] =
{  
  {"install",   'i', 0, 0,"Will start the installation process."},
  {"update",   'u', 0, 0,"Update to reflect current File system state"},
  {"recover", 'g', 0, 0, "Undelete/recover files"},
  {"uninstall",'q',0,0,"Uninstalls giis-ext4"},
  {"list",'l',0,0,"List deleted files"},
  {0}
};

int just_list;//display files
int max_dir_depth; //will be set from giis header
int update_time,update;
sqlite3 *conn;
int EXT2_BLOCK_SIZE;
int date_mode=-1,day,month,year,day1,month1,year1;
int is_file_already_exists;
int dp;
char cwd[40];
const char *argp_program_version = "giis-ext4 1.0 (06-11-2012) ";
const char *argp_program_bug_address = "<http://groups.google.com/group/giis-users>";


/* Functions involved. */
static error_t parse_opt (int, char *, struct argp_state *); 
int  giis_ext4_parse_dir(int, char *,unsigned long,ext2_filsys);
int giis_ext4_dump_data_blocks(struct giis_recovered_file_info *,ext2_filsys);
int giis_ext4_list_file_details(struct giis_recovered_file_info *,ext2_filsys);
int giis_ext4_sqlite_insert_record(struct linux_dirent *,struct ext2_inode *,unsigned long,int,char []);
int giis_ext4_recover_all(ext2_filsys ,int );
int giis_ext4_write_into_file(struct giis_recovered_file_info *,unsigned char []);
int giis_ext4_search4fs (char *);
int giis_ext4_log_mesg(char *,char *,char *);
int giis_ext4_get_date();
int giis_ext4_check_ddate(struct ext2_inode *);
int giis_ext4_creat_tables(char *);
int giis_ext4_update_dirs(ext2_filsys );
unsigned long getinodenumber(char *);
void giis_ext4_open_db();
void giis_ext4_close_db();
int giis_ext4_sqlite_verify_record(unsigned long);
int giis_ext4_uninstall(void);
static int giis_ext4_unlock_db(int fd ,int offset,int len);
static int giis_ext4_lock_db(int fd ,int offset,int len);


static char args_doc[] = "";
static char doc[] = "giis-ext4 - An undelete tool for ext4 file system.(http://www.giis.co.in)";
static struct argp argp = {options, parse_opt, args_doc, doc};


static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'i':
      arguments->flag=1;
      break;
    case 'u':
      arguments->flag=2;
      break;
    case 'g':
      arguments->flag=3;
      break;
    case 'q':
      arguments->flag=4;
      break;
    case 'l':
      arguments->flag=5;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


int main(int argc,char *argv[]){
ext2_filsys	current_fs = NULL;
char device[75];
int  open_flags = EXT2_FLAG_SOFTSUPP_FEATURES | EXT2_FLAG_RW;
blk_t superblock=0;
blk_t blocksize=0;
int retval=0;
int i=0;
int ans=0;
struct arguments arguments;
argp_parse (&argp, argc, argv, 0, 0, &arguments);
if ( !(arguments.flag>0 && arguments.flag<6))
  handle_error("For usage type : giis-ext4 --help");

if(arguments.flag==4){
giis_ext4_uninstall();
exit(0);
}

//get device name
retval=giis_ext4_search4fs (device);
if(retval == -1){
		handle_error("Root File System not Found.Exiting.");
		}	

retval = ext2fs_open(device, open_flags, superblock, blocksize,unix_io_manager, &current_fs);
if (retval) {
		current_fs = NULL;
		handle_error("Error while opening filesystem.");
	}

EXT2_BLOCK_SIZE=current_fs->blocksize;

if(arguments.flag == 1){
printf("\n giis : Taking snapshot of current File system \n");
update=FALSE;
giis_ext4_creat_tables(device);
giis_ext4_update_dirs(current_fs);

printf("\n *Please add following entry into your /etc/crontab file for auto update");
printf("\n */%d * * * * root /usr/bin/giis-ext4 -u > /dev/null ",update_time);
printf("\n giis-ext4:Installation is complete.\n"); 
}else if (arguments.flag == 2){
printf("\n giis : Updating snapshot of current File system \n");
update=TRUE;
giis_ext4_open_db();
giis_ext4_update_dirs(current_fs);
printf("\n giis-ext4:Update is complete.\n"); 
}else if (arguments.flag == 3){
printf("\n press 1: get all user files");printf("\n press 2: get specific user files");
printf("\n press 3: get specific file type");printf("\n press 4: get specific file");
printf("\n press 5: get it by deleted date");printf("\n Enter your option:");
scanf("%d",&ans);

giis_ext4_recover_all(current_fs,ans);
printf("\n\n **giis-ext4 : Recovery completed.Please check %s for more details and %s for files **\n",GIIS_LOG_FILE,RESTORE_DIR);
exit(0);
}else if (arguments.flag == 5){
just_list=1;
giis_ext4_recover_all(current_fs,1);
}
ext2fs_close(current_fs);
return 1;
}

int  giis_ext4_parse_dir(int depth, char *gargv,unsigned long parent_inode,ext2_filsys current_fs)
{
	int fd, nread,i;
	char buf[BUF_SIZE];
	struct linux_dirent *d;
	struct ext2_inode inode,*in;
	int bpos;
	char d_type;
	char dir[512]={0};
	char	*pathname=NULL;
	extern int max_dir_depth,update,update_time;
	in = &inode;

	memset(dir,'\0',512);
	strcpy(dir,gargv);
	
	fd = open(dir, O_RDONLY );
	if (fd == -1){
		printf("%s",dir);
	handle_error("open");
	}

	printf("\n Parsing directory  : %s",dir);


if(depth < max_dir_depth){
	for ( ; ; ) {
	nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
	if (nread == -1)
		handle_error("getdents");

	if (nread == 0)
		break;

	for (bpos = 0; bpos < nread;) {
		d = (struct linux_dirent *) (buf + bpos);
		d_type = *(buf + bpos + d->d_reclen - 1);


			//read inode stats
			ext2fs_read_inode(current_fs,d->d_ino,in);
			//inode-stats ends
		if(d_type == DT_DIR){
			//skip system files too
			if(strcmp(".",d->d_name)==0 || strcmp("..",d->d_name)==0 ){
			bpos += d->d_reclen;
			continue;
			}

			if(d->d_name[0]=='.'){
			bpos += d->d_reclen;
			continue;
			}
				//printf("\n -->checking dir:%s -mtime=%u  ctime=%u",d->d_name,in->i_mtime,in->i_ctime);
				if(giis_ext4_sqlite_verify_record_for_directory(d->d_ino,in->i_mtime,in->i_ctime)){
					//if time-stamp are same - If its update && its not new directory - then skipdir
					//if time-stamp are - during install - don't skipdir.
					if(update==TRUE && (giis_ext4_sqlite_verify_record(d->d_ino)==0)){
					printf("\n\tskipdir   :  %s",d->d_name);
					goto skipdir;
					}
					printf("\n\tupdatedir : %s",d->d_name);
				}
				chdir(dir);
				//If its directory insert the record too.
					if (update==TRUE) { 
					//	if(in->i_mtime > (time(0)-(update_time*60))) {
						//check whether this entry already exists
						 if(giis_ext4_sqlite_verify_record(d->d_ino)){
						//convert inode into absoulte pathname
						pathname=NULL;
						ext2fs_get_pathname (current_fs, parent_inode, d->d_ino, &pathname);
						giis_ext4_sqlite_insert_record(d,in,parent_inode,depth,pathname);
						 }

					//	}

					}
					
					if(update==FALSE){
					//convert inode into absoulte pathname
					pathname=NULL;
					ext2fs_get_pathname (current_fs, parent_inode, d->d_ino, &pathname);
					giis_ext4_sqlite_insert_record(d,in,parent_inode,depth,pathname);
					}
				//parse this directory, if and only if mtime or ctime changed.
				giis_ext4_parse_dir(depth+1,d->d_name,d->d_ino,current_fs);
			
			}else
			{
				if (d_type == DT_REG){
					if (update==TRUE) { 
						if(in->i_mtime > (time(0)-(update_time*60))) {
						//check whether this entry already exists
						 if(giis_ext4_sqlite_verify_record(d->d_ino)){
						//convert inode into absoulte pathname
						pathname=NULL;
						ext2fs_get_pathname (current_fs, parent_inode, d->d_ino, &pathname);
						giis_ext4_sqlite_insert_record(d,in,parent_inode,depth,pathname);
						 }

						}

					}
					
					if(update==FALSE){
					//convert inode into absoulte pathname
					pathname=NULL;
					ext2fs_get_pathname (current_fs, parent_inode, d->d_ino, &pathname);
					giis_ext4_sqlite_insert_record(d,in,parent_inode,depth,pathname);
					}
			
				}	
			}		
		skipdir:
		bpos += d->d_reclen;
			
	}
	}
}
	chdir("..");
	close(fd);			
	
	return 0;
}

/*
gets struct with file details fetched from backend and dumps content from its blocks 
*/
int giis_ext4_dump_data_blocks(struct giis_recovered_file_info *fi,ext2_filsys current_fs){
	unsigned long	blk;
	unsigned char buf[EXT2_BLOCK_SIZE];
	unsigned int	i;
	errcode_t	retval;
	int total_blks=0;
	extern int is_file_already_exists;
	char file_location[512]={0};

	char md5_cmd[512],md5sum[34];
	FILE *pf;

		is_file_already_exists=0;
		if(open (fi->fpath, O_CREAT|O_EXCL, S_IRWXU |S_IRWXG |S_IRWXO) == -1 )
			is_file_already_exists=1;

		i=0;
		while(fi->extents[i] && i < 4 ){
			/* Set extents length and start block */
			total_blks=fi->extents[i];
			blk=fi->starting_block[i];
			while(total_blks > 0){		
				retval = io_channel_read_blk(current_fs->io, blk, 1, buf);
					if (retval) {
					handle_error("giis_ext4_dump_data_blocks::io_channel_read_blk:Can't read from block");
					return;
					}
				/* Write data into file */
					retval=giis_ext4_write_into_file(fi,buf);
					if(retval== -1){
						fprintf(stderr,"\n Error while recovering file %s , so skipping it",fi->fpath);
						return 1;
					}
				blk++;
				total_blks --;
			}
		i++;
		}
		/* log file name and path */
		if(is_file_already_exists){
		strcpy(file_location,RESTORE_DIR);
		strcat(file_location,fi->fname);
		}else{
		strcpy(file_location,fi->fpath);
		}
		
		//set correct file-size
		truncate(file_location,fi->fsize);	
		//Recompute md5  of recovered file
		memset(md5_cmd,'\0',512);
		sprintf(md5_cmd,"md5sum %s",file_location);
	
		pf=popen(md5_cmd,"r");
		if(!pf){
		fprintf(stderr,"Could not open pipe");
		return ;
		}
	
		//get data
        	 fgets(md5sum, 34 , pf);
                                                  
	         if (pclose(pf) != 0)
        	 fprintf(stderr," Error: close Failed.");

		fprintf(stdout,"Md5sum is %s",md5sum);

		giis_ext4_log_mesg(file_location,fi->md5sum,md5sum);
		
return 1;
}
int giis_ext4_sqlite_insert_record(struct linux_dirent *d1,struct ext2_inode *inode,unsigned long parent_inode,int depth,char cwd[]){
	extern sqlite3 *conn;
	sqlite3_stmt    *Stmt;
	int     error = 0;
	char *dbfile=SQLITE_DB_LOCATION;
	char  *errmsg;
	const char *zLeftover;
	time_t result;					/* time n date of when this record updated into db */
	char md5_cmd[512],md5sum[34];
	FILE *pf;
		
		
	error = sqlite3_prepare(conn,SQL_STMT_INSERT_TABLE, -1, &Stmt, &zLeftover);
	assert(error == SQLITE_OK);



	error = sqlite3_bind_text(Stmt, 1,d1->d_name, strlen(d1->d_name), SQLITE_STATIC);assert(error == SQLITE_OK);
	error = sqlite3_bind_int64(Stmt, 2,d1->d_ino);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int64(Stmt, 3,parent_inode);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 4,inode->i_mode);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 5,inode->i_uid);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 6,inode->i_flags);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int64(Stmt, 7,inode->i_size);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 8,inode->i_mode);assert(error == SQLITE_OK);
	error = sqlite3_bind_text(Stmt, 9,cwd, strlen(cwd), SQLITE_STATIC);assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 10,inode->i_gid);assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 11,depth);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 12,inode->i_block[4]);   assert(error == SQLITE_OK);

	//If its directory field 13(blk1),15(blk2) will have mtime and ctime 
	if (S_ISREG(inode->i_mode)){
	error = sqlite3_bind_int64(Stmt, 13,inode->i_block[5]);   assert(error == SQLITE_OK);
	}
	else{
	error = sqlite3_bind_int64(Stmt, 13,inode->i_mtime);   assert(error == SQLITE_OK);
	}
	
	
	error = sqlite3_bind_int(Stmt, 14,inode->i_block[7]);   assert(error == SQLITE_OK);

	if (S_ISREG(inode->i_mode)){
	error = sqlite3_bind_int64(Stmt, 15,inode->i_block[8]);   assert(error == SQLITE_OK);
	}
	else{
	error = sqlite3_bind_int64(Stmt, 15,inode->i_ctime);   assert(error == SQLITE_OK);
	}
	
	error = sqlite3_bind_int(Stmt, 16,inode->i_block[10]);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int64(Stmt, 17,inode->i_block[11]);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int(Stmt, 18,inode->i_block[13]);   assert(error == SQLITE_OK);
	error = sqlite3_bind_int64(Stmt, 19,inode->i_block[14]);   assert(error == SQLITE_OK);
	//compute md5 
	if (S_ISREG(inode->i_mode)){
	
	memset(md5_cmd,'\0',512);
	sprintf(md5_cmd,"md5sum %s",cwd);
	
	pf=popen(md5_cmd,"r");
	if(!pf){
		fprintf(stderr,"Could not open pipe");
		return ;
		}
	
	//get data
         fgets(md5sum, 34 , pf);
                                                  
         if (pclose(pf) != 0)
         fprintf(stderr," Error: close Failed.");
	error = sqlite3_bind_text(Stmt, 20,md5sum, 34, SQLITE_STATIC);assert(error == SQLITE_OK);
	}
	

	up:
	error = sqlite3_step(Stmt); 
	if(error==SQLITE_BUSY){
	printf("\n data base is busy wait 20 seconds");
	printf("\n ->>%d<<-",error);
	sleep(20);
	goto up;
	}

	assert(error == SQLITE_DONE);
	error = sqlite3_reset(Stmt);                assert(error == SQLITE_OK);
	sqlite3_finalize(Stmt);

}

/* giis_ext4_recover_all : Undelete all files from all users */
int giis_ext4_recover_all(ext2_filsys current_fs,int option){
	extern sqlite3 *conn;
	sqlite3_stmt    *res,*Stmt;
	int     error = 0,uid=-1;
	int     rec_count = 0;
	const char      *errMSG;
	const char      *tail;
	char  *errmsg,*buf;
	char *dbfile=SQLITE_DB_LOCATION;
	const char *zLeftover;
	struct ext2_inode inode,*in;
	struct passwd *pwfile,pwd;  			 /* Used for uid verfication */
	char user[50],file[100],extention[25];
	int retval=0;
    
		fi=&s_giis_recovered_file_info;
		in=&inode;

		//get the connection
		giis_ext4_open_db();
		if (option == 5){//get by deleted date
		giis_ext4_get_date();
		option =1 ; //and start recovery all
		}
		if(option == 1){
		//recover all
		error = sqlite3_prepare_v2(conn,SQL_STMT_GET_ALL,-1, &res, &tail);
		}else if(option == 2){
		//recover specific user
		buf = malloc(8192);
		if (buf == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
		}

		printf ("\n Enter the User Name....");
		scanf ("%s", user);
		error = getpwnam_r(user, &pwd, buf, 8192, &pwfile);
		if (pwfile == NULL)
		{
		handle_error("Please enter valid user name");
		}
		
		uid=pwd.pw_uid;
		
		error = sqlite3_prepare_v2(conn,SQL_STMT_GET_USR,-1, &res, &tail);
		error = sqlite3_bind_int(res,1,uid);   assert(error == SQLITE_OK);
		}else if (option == 3){
		printf("\n Make sure you use \% before extentions - sql injection :) ");
		puts("\n Enter the file extention  ( %.txt or  %.c or %.cpp ...) :");
		scanf("%s",extention);

		error = sqlite3_prepare_v2(conn,SQL_STMT_GET_FTYPE,-1, &res, &tail);
		error = sqlite3_bind_text(res, 1,extention,strlen(extention), SQLITE_STATIC);   assert(error == SQLITE_OK);
		}else if (option == 4){
		printf ("\n Enter the Filename Name....");
		scanf ("%s", file);
		error = sqlite3_prepare_v2(conn,SQL_STMT_GET_FILE,-1, &res, &tail);
		error = sqlite3_bind_text(res, 1,file,strlen(file), SQLITE_STATIC);   assert(error == SQLITE_OK);
		}


		if (error != SQLITE_OK) {
		handle_error("No matching record found.");
		}
		printf("\n Verifing inode:");
		while (sqlite3_step(res) == SQLITE_ROW) {
		
			fi->fname=sqlite3_column_text(res, 0);

			fi->inode_num=sqlite3_column_int64(res, 1);
			if(!just_list)
			printf("%lu|",fi->inode_num);

			fi->extents[0]=sqlite3_column_int(res, 2);
			fi->starting_block[0]=sqlite3_column_int64(res, 3);

			fi->extents[1]=sqlite3_column_int(res,4);
			fi->starting_block[1]=sqlite3_column_int64(res, 5);

			fi->extents[2]=sqlite3_column_int(res, 6);
			fi->starting_block[2]=sqlite3_column_int64(res, 7);

			fi->extents[3]=sqlite3_column_int(res, 8);
			fi->starting_block[3]=sqlite3_column_int64(res, 9);

			fi->fsize=sqlite3_column_int64(res, 10);
		
			fi->fpath=sqlite3_column_text(res, 11);
			
			fi->mode=sqlite3_column_int(res, 12);
			
			fi->owner=sqlite3_column_int(res, 13);
			fi->group=sqlite3_column_int(res, 14);
			fi->md5sum=sqlite3_column_text(res, 15);

			ext2fs_read_inode(current_fs,fi->inode_num,&inode);

			if (in->i_links_count ==0 && S_ISREG(fi->mode)){
				if(date_mode !=-1){
				if((giis_ext4_check_ddate(in)==1) && (fi->starting_block[0])){
					if(!just_list)
					giis_ext4_dump_data_blocks(fi,current_fs);
					else
					giis_ext4_list_file_details(fi,current_fs);
				}
				}else{//Not date mode
				if(fi->starting_block[0])
					if(!just_list)
					giis_ext4_dump_data_blocks(fi,current_fs);
					else
					giis_ext4_list_file_details(fi,current_fs);
				}
			} 

		
		rec_count++;
	}


		error = sqlite3_reset(res);assert(error == SQLITE_OK);
		sqlite3_finalize(res);
		giis_ext4_close_db();

}
int giis_ext4_write_into_file(struct giis_recovered_file_info *fi,unsigned char buf[EXT2_BLOCK_SIZE]){
int fp;
int retval=0;
char name[275] = RESTORE_DIR;
extern int is_file_already_exists;

if(is_file_already_exists){
strcat (name, fi->fname);
fp = open (name, O_CREAT|O_APPEND | O_RDWR ,S_IRWXU |S_IRWXG |S_IRWXO);
chmod(name,fi->mode);
chown(name,fi->owner,fi->group);
}else{
fp = open (fi->fpath, O_CREAT|O_APPEND | O_RDWR ,S_IRWXU |S_IRWXG |S_IRWXO);
chmod(fi->fpath,fi->mode);
chown(fi->fpath,fi->owner,fi->group);
}

if (fp == -1)
{
close (fp);
fprintf(stderr,"giis_ext4_write_into_file::Cannot open file %s",fi->fpath);
return -1;
}
	//puts(buf);
retval=write(fp,buf,EXT2_BLOCK_SIZE);
if ( retval == -1){
close (fp);
fprintf(stderr,"giis_ext4_write_into_file::Cannot write into file");
return -1;
	}		
close(fp);
return 1;

}
/* giis_ext4_search4fs : Taken from giis/init.c */
int giis_ext4_search4fs (char *device)
{
FILE *fd;
char *line=NULL;
char *wordptr=NULL;
int bytes=180;
fd = fopen ("/etc/mtab", "r");
if (fd == NULL)
{
handle_error("giis_ext4_search4fs::unable to open /etc/mtab");
}

	while (fd !=NULL){
		if (getline(&line,&bytes,fd) == -1){
			//puts("EOF");
			return -1;
		}

		if (strstr(line,"/ ext4 ") !=NULL){
			wordptr = strtok(line," "); 
			strcpy(device,wordptr);
			printf("\n Device Found : %s",device);
			return 1;
			
		}			
	}
return -1;
}
/* log file */
int giis_ext4_log_mesg(char *mesg,char *md5sum,char *new_md5sum){
int fp;
struct tm mytm;
time_t result;
char line[256];
result=time(NULL);

fp=open(GIIS_LOG_FILE,O_RDWR |O_CREAT| O_APPEND,S_IRWXU |S_IRWXG |S_IRWXO);

memset(line,'\0',256);
sprintf(line,"\nFile Name : %s\n",mesg);
write(fp,line,strlen(line));

memset(line,'\0',256);
sprintf(line,"Old MD5SUM : %s\n",md5sum);
write(fp,line,strlen(line));

memset(line,'\0',256);
sprintf(line,"New MD5SUM : %s\n",new_md5sum);
write(fp,line,strlen(line));

memset(line,'\0',256);
if(strcmp(md5sum,new_md5sum)==0)
sprintf(line,"MD5SUM Match:Yes \n");
else
sprintf(line,"MD5SUM Match:No \n");
write(fp,line,strlen(line));


memset(line,'\0',256);
sprintf(line,"Recovered on :%s\n",ctime(&result));
write(fp,line,strlen(line));

close(fp);
return 1;
}
//
int giis_ext4_get_date(){
	extern int date_mode,day,month,year,day1,month1,year1; /* Time based recovery */
	date_mode=-1;
	printf("\n\nGet Files by Deleted Date:\n\tPress 0 : Deleted on\n\tPress 1 : Deleted After\n\tPress 2 : Deleted Before \n\tPress 3 : Deleted Between");
	printf("\n\n\t\tEnter Your Choice :");
	scanf("%d",&date_mode);

	if((date_mode<0) || (date_mode>3)){
	handle_error("\n Please Enter Valid Choice.");
	}

	if(date_mode >= 0 && date_mode <= 3 ){
	printf("\n Enter date1: DD MM YYYY :");
	scanf("%d %d %d",&day,&month,&year);
	}
	if(date_mode == 3){
	printf("\n Enter date2: DD MM YYYY :");
	scanf("%d %d %d",&day1,&month1,&year1);
	}
	// date validation
	if(((day<=0 || day>31)||(month<=0 || month>12)||(year<1000))||((date_mode == 3)&&(day1<=0 || day1>31 || month1<=0 || month1>12|| year1<1000 )))
	{
	handle_error("\n Please Enter Valid Date.");
	}
}

int giis_ext4_check_ddate(struct ext2_inode *inode){
	extern int date_mode,day,month,year,day1,month1,year1; /* Time based recovery */
	time_t result,result1;					/* time n date  used for Time based recovery*/
	struct tm mytm = { 0 };

	mytm.tm_year = year - 1900;
	mytm.tm_mon = month - 1;
	mytm.tm_mday = day;
	result = mktime(&mytm);
	if (result == (time_t) -1) {
	handle_error ("\n Time computation failed");
	} 

	//Deleted on 
	if(date_mode ==0){
	if ((inode->i_dtime >= result) && (inode->i_dtime<= (result+86400)))
	return 1;
	else
	return 0;
	}

	//Deleted before
	if(date_mode ==2){
	if (inode->i_dtime<result)
	return 1;
	else
	return 0;
	}	

	//Deleted after 

	if(date_mode==1){
	if (inode->i_dtime > (result+86400))
	return 1;
	else
	return 0;
	}

	//Deleted between 
	if(date_mode==3){
	//set to-date
	mytm.tm_year = year1 - 1900;
	mytm.tm_mon = month1 - 1;
	mytm.tm_mday = day1;
	result1 = mktime(&mytm);


	if ((inode->i_dtime > result) && (inode->i_dtime < (result1+86400)))
	return 1;
	else
	return 0;
	}

}

int giis_ext4_creat_tables(char *device){
	extern sqlite3 *conn;
	sqlite3_stmt    *Stmt;
	int     error = 0;
	char *dbfile=SQLITE_DB_LOCATION;
	char  *errmsg;
	const char *zLeftover;
	time_t result;					/* time n date of when this record updated into db */
	int count;		
	int ans,max_depth,update_time;
	char dirname[8][512]={0};
	
		// Creat dirs
		if(mkdir(GIISDIR,0700)==-1)
		    handle_error ("\n mkdir: giisdir failed");
		if(mkdir (SQLITE_DB_DIR,0700)==-1)
		 handle_error ("\n mkdir: dbdir failed");
		if(mkdir (RESTORE_DIR,0700)==-1)
		 handle_error ("\n mkdir: resultdir failed");
		if(creat (GIIS_LOG_FILE,0700)==-1)
		 handle_error ("\n creat: logpath failed");

		//get connection
		giis_ext4_open_db();
		printf("\n giis-ext4:Installation begins..");
		//Creat giisheader table
		error = sqlite3_exec(conn,SQL_STMT_CREATE_HEADER,0,0,0);

		if (error) {
			fprintf(stderr, "Can not create table:  \n" );
			exit(0);
		}else{
			printf("\n giis-ext4: header table created");
			}



		//create table
		error = sqlite3_exec(conn,SQL_STMT_CREATE_TABLE,0,0,0);

		if (error) {
			fprintf(stderr, "Can not create table:  \n" );
			exit(0);
		}else{
			printf("\n giis-ext4: file table created");
			}
		printf("\n What's the maximum directory depth?");
		scanf("%d",&max_depth);
		printf("\n Enter the dirname name,that you would like to protect(Max. 7 directories)");
		count=0;
		do{
		printf("\n Enter dirname:");
		scanf("%s",dirname[count]);
		printf("\n Press 1 to add/protect another directory else Press 0 to complete: ");
		scanf("%d",&ans);
		count++;
		}while(count<7 && (ans != 0) );


		printf("\n Check for newly files every 'auto update time' minutes.\nEnter auto update time: ");
		scanf("%d",&update_time);


		/*Now insert the records into giis header table */
		error = sqlite3_prepare(conn,SQL_STMT_INSERT_HEADER, -1, &Stmt, &zLeftover);
		assert(error == SQLITE_OK);

		error = sqlite3_bind_int(Stmt, 1,max_depth);   assert(error == SQLITE_OK);
		error = sqlite3_bind_int(Stmt, 2,update_time);   assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 3,device, strlen(device), SQLITE_STATIC);assert(error == SQLITE_OK);
	
		error = sqlite3_bind_text(Stmt, 4,dirname[0], strlen(dirname[0]), SQLITE_STATIC);assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 5,dirname[1], strlen(dirname[1]), SQLITE_STATIC);assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 6,dirname[2], strlen(dirname[2]), SQLITE_STATIC);assert(error == SQLITE_OK);

		error = sqlite3_bind_text(Stmt, 7,dirname[3], strlen(dirname[3]), SQLITE_STATIC);assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 8,dirname[4], strlen(dirname[4]), SQLITE_STATIC);assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 9,dirname[5], strlen(dirname[5]), SQLITE_STATIC);assert(error == SQLITE_OK);
		error = sqlite3_bind_text(Stmt, 10,dirname[6], strlen(dirname[6]), SQLITE_STATIC);assert(error == SQLITE_OK);

	

		error = sqlite3_step(Stmt);                 assert(error == SQLITE_DONE);
		error = sqlite3_reset(Stmt);                assert(error == SQLITE_OK);


		sqlite3_finalize(Stmt);


}
int giis_ext4_update_dirs(ext2_filsys current_fs){
	extern sqlite3 *conn;
	sqlite3_stmt    *Stmt;
	int     error = 0;
	char *dbfile=SQLITE_DB_LOCATION;
	char  *errmsg;
	const char *zLeftover;
	time_t result;					/* time n date of when this record updated into db */
	int count;		
	int ans;
	char dirname[8][512]={0};
	sqlite3_stmt    *res;
	const char      *tail;
	
	extern int max_dir_depth,update_time;
		
	extern struct giis_protected_dir_info	s_giis_protected_dir_info;
	di=&s_giis_protected_dir_info;


	error = sqlite3_prepare_v2(conn,SQL_STMT_GET_DIRNAMES,-1, &res, &tail);
	if (error != SQLITE_OK) {
		handle_error("No matching record found.");
	}


	if (sqlite3_step(res) == SQLITE_ROW) {
			di->max_depth=sqlite3_column_int(res, 0);
			di->update_time=sqlite3_column_int(res, 1);
			di->device_name=sqlite3_column_text(res, 2);
			
			di->protected_dir[0]=sqlite3_column_text(res, 3);
			di->protected_dir[1]=sqlite3_column_text(res, 4);
			di->protected_dir[2]=sqlite3_column_text(res, 5);
			di->protected_dir[3]=sqlite3_column_text(res, 6);

			di->protected_dir[4]=sqlite3_column_text(res, 7);
			di->protected_dir[5]=sqlite3_column_text(res, 8);
			di->protected_dir[6]=sqlite3_column_text(res, 9);
			

			//set max_depth 
			max_dir_depth=di->max_depth;
			//set auto update
			update_time=di->update_time;

			count=0;
			while(strlen(di->protected_dir[count])!=0){
				giis_ext4_parse_dir(1,di->protected_dir[count],getinodenumber(di->protected_dir[count]),current_fs);
				count++;

			}

	}

				sqlite3_finalize(res);
				giis_ext4_close_db();

}

unsigned long getinodenumber(char *path){
	  struct stat statbuf;
	  unsigned long parent;
	  
	    if (stat(path, &statbuf) != -1)
	    parent=statbuf.st_ino;
	    return parent;
}

void giis_ext4_close_db(){
extern sqlite3 *conn;

	close2:
	while(sqlite3_close(conn)==SQLITE_BUSY){
	printf("\n Db not closed");
	sleep(5);
	goto close2;
	}
		
	if(update==TRUE){
		if (giis_ext4_unlock_db(dp,0,0) !=0)
		printf("unlock failed");
	}
}

static int giis_ext4_lock_db(int fd,int offset,int len){
struct flock lock;
lock.l_type=F_WRLCK;
lock.l_whence=SEEK_SET;
lock.l_start=offset,
lock.l_len=len;
lock.l_pid=0;
return fcntl(fd,F_SETLKW,&lock);
}
static int giis_ext4_unlock_db(int fd,int offset,int len){
struct flock lock;
lock.l_type=F_UNLCK;
lock.l_whence=SEEK_SET;
lock.l_start=offset,
lock.l_len=len;
lock.l_pid=0;
return fcntl(fd,F_SETLKW,&lock);
}

static int giis_ext4_get_lock(int fd,int offset,int len){
struct flock lock;
lock.l_type=F_RDLCK;
lock.l_whence=SEEK_SET;
lock.l_start=offset,
lock.l_len=len;
lock.l_pid=0;
return fcntl(fd,F_GETLK,&lock);
}

void giis_ext4_open_db(){
extern sqlite3 *conn;
int     error = 0;
char *dbfile=SQLITE_DB_LOCATION;
extern int dp;
	
	if(update==TRUE){
		dp=open(dbfile,O_RDWR);

		if (giis_ext4_lock_db(dp,0,0) !=0){
		printf("lock failed");
		giis_ext4_get_lock(dp,0,1);
		}
	}
	


	error = sqlite3_open(dbfile, &conn);
	assert(error == SQLITE_OK);
	if (error) {
				handle_error("Can not open database");
	}
}
//todo:this function currently just check whether record exists or not.
int giis_ext4_sqlite_verify_record(unsigned long number){
  int     error = 0;
  sqlite3_stmt    *Stmt;
  const char      *tail;
	error = sqlite3_prepare_v2(conn,SQL_STMT_VERIFY_INODE,-1, &Stmt, &tail);
	error = sqlite3_bind_int64(Stmt,1,number);   assert(error == SQLITE_OK);
	if (error != SQLITE_OK) {
		 printf("No matching record found");
		 return 1;
	}
	//printf("\ninode<%lu>",number);
	if (sqlite3_step(Stmt) == SQLITE_ROW){
	  //printf("Record already exists");   
	  sqlite3_finalize(Stmt);
	  return 0;
	}
	else{
	//  printf("No Record already exists");     
	  sqlite3_finalize(Stmt);
	  return 1;
	}
	
}
int giis_ext4_sqlite_verify_record_for_directory(unsigned long number,unsigned long mtime,unsigned long ctime){
  int     error = 0;
  sqlite3_stmt    *Stmt;
  const char      *tail;
  unsigned long db_ctime,db_mtime;
	error = sqlite3_prepare_v2(conn,SQL_STMT_VERIFY_INODE,-1, &Stmt, &tail);
	error = sqlite3_bind_int64(Stmt,1,number);   assert(error == SQLITE_OK);
	if (error != SQLITE_OK) {
		 printf("No matching record found");
		 return 1;
	}
//	printf("\ninode<%lu>",number);
	if (sqlite3_step(Stmt) == SQLITE_ROW){
//	  printf("Record already exists");   
          db_mtime=sqlite3_column_int64(Stmt, 12);
          db_ctime=sqlite3_column_int64(Stmt, 14);
	  sqlite3_finalize(Stmt);
//	  printf ("\n db %u %u %u %u",db_mtime,db_ctime);

	  if ((db_mtime == mtime) && (db_ctime == ctime))
	  return 1;
	  else
	  return 0;
		
	}
	else{
//	  printf("No Record already exists");     
	  sqlite3_finalize(Stmt);
	  return 1;
	}
	
}
//this function will remove giis-ext4
int giis_ext4_uninstall(){
int retval=0;
  printf("\n\t Press 1 to uninstall:");
  scanf("%d",&retval);
  if(retval==1){
	
  retval=unlink (SQLITE_DB_LOCATION);
  if (retval == 0)
    printf ("\n\t%s Removed",SQLITE_DB_LOCATION);
  else
    printf ("\n\t%s not deleted - please remove it manually.",SQLITE_DB_LOCATION);

  retval=unlink (GIIS_LOG_FILE);
  if (retval == 0)
    printf ("\n\t%s Removed.",GIIS_LOG_FILE);
  else
    printf ("\n\t%s not deleted - please remove it manually.",GIIS_LOG_FILE);

  retval=rmdir (RESTORE_DIR);
  if (retval == 0)
    printf ("\n\t%s Removed.",RESTORE_DIR);
  else
    printf ("\n\t%s not deleted - please remove it manually.",RESTORE_DIR);


  retval=rmdir (SQLITE_DB_DIR);
  if (retval == 0)
    printf ("\n\t%s Removed.",SQLITE_DB_DIR);
  else
    printf ("\n\t%s not deleted - please remove it manually.",SQLITE_DB_DIR );

  retval=rmdir (GIISDIR);
  if (retval == 0)
    printf ("\n\t%s Removed.",GIISDIR);
  else
    printf ("\n\t%s not deleted - please remove it manually.",GIISDIR );

 retval=unlink ("/usr/bin/giis-ext4");
  if (retval == 0)
    printf ("\n\tgiis-ext4 binary Removed.");
  else
    printf ("\n\tgiis-ext4 not deleted - please remove it manually.");

   printf("\n giis-ext4: cleaned up - Please remove giis-ext4 related entry from crontab file\n");
   printf("\n Don't forgot to take backups!! Good luck :)\n\n ");
  }
}
  
int giis_ext4_list_file_details(struct giis_recovered_file_info *fi,ext2_filsys current_fs){
	printf("\nFile:%s was deleted from %s.\n",fi->fname,fi->fpath);
	return 1;
}
	
