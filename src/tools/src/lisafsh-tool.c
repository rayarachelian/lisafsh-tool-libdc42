/**************************************************************************************\
*                   A part of the Apple Lisa 2 Emulator Project                        *
*                                                                                      *
*                  Copyright (C) 1998, 2010 Ray A. Arachelian                          *
*                            All Rights Reserved                                       *
*                                                                                      *
*                     Release Project Name: LisaFSh Tool                               *
*                                                                                      *
\**************************************************************************************/

#include <sys/stat.h>
#include <sys/types.h>

#include "libdc42.h"

const uint16 dispsize=16;

int tagsaresorted=0;
uint32 sector=0;
uint32 clustersize=512;
uint32 havetags=0L;

char cmd_line[8192];                        // command line

typedef struct
{
  int sector;                               // which sector is this?
  char *tagptr;
} sorttag_type;


int    *sorttag=NULL;                       // sorted tags
uint8 *allocated=NULL;

DC42ImageType F;

char volumename[32];
uint16 fsversion=0;
uint32 firstmddf=65536;

char filenames[65536][2][64];         // [fileid][size of file name][2] 0=Lisa name 1=sanitized name
                                      // cached file names for all the tags - kind of big, I know.

char directory[65536];
// the first few sectors of a disk are reserved for the boot sector + os loader
#define RESERVED_BLOCKS 28

#define TAG_BOOTSECTOR 0xaaaa
#define TAG_OS_LOADER  0xbbbb
#define TAG_FREE_BLOCK 0x0000
#define TAG_ERASED_BLK 0x7fff

#define TAG_MDDF       0x0001
#define TAG_FREEBITMAP 0x0002
#define TAG_S_RECORDS  0x0003
#define TAG_DIRECTORY  0x0004
#define TAG_MAXTAG     0x7fff

#define MAXARGS 640
// command line user interface verbs  ////////////////////////////////////////////////////////////////
int lastarg=0;

char cargsstorage[8192];

long  iargs[MAXARGS];
long  hargs[MAXARGS];
long  dargs[MAXARGS];
char *cargs[MAXARGS];



enum command_enum {nullcmd=-999, secprevcmd=-2, secnxt=-1,
                   sectorset=0,    cluster=1, display=2, setclustersize=3, dump=4, tagdump=5,
                   sorttagdump=6, sortdump=7, extract=8,  version=9, help=10,  bitmap=11,
                   sortnext=12,sortprevious=13,dir=14,    editsector=15, edittag=16, diff2img=17,
                   loadsec=18, loadbin=19, loadprofile=20, volname=21, quit=22             };
enum command_enum command;

#define QUITCOMMAND 22
#define LASTCOMMAND 23

char *cmdstrings[LASTCOMMAND+2] =
                  {"sector",       "cluster","display", "setclustersize", "dump",     "tagdump",
                   "sorttagdump",  "sortdump","extract", "version",       "help",     "bitmap",
                   "n",             "p",      "dir",     "editsector",    "edittag",  "difftoimg",
                   "loadsec",  "loadbin", "loadprofile",  "volname", "quit", ""};

#define NULL_CMD         -999
#define SECTOR_PRV         -2
#define SECTOR_NXT         -1

#define SECTOR_CMD          0
#define CLUSTER_CMD         1
#define DISPLAY_CMD         2
#define SETCLUSTERSIZE_CMD  3
#define DUMP_CMD            4
#define TAGDUMP_CMD         5

#define SORTTAGDUMP_CMD     6
#define SORTDUMP_CMD        7
#define EXTRACT_CMD         8
#define VERSION_CMD         9
#define HELP_CMD           10
#define BITMAP_CMD         11

#define SORT_NEXT          12
#define SORT_PREV          13
#define DIR_CMD            14
#define EDITSECTOR_CMD     15
#define EDITTAG_CMD        16

#define DIFF2IMG_CMD       17
#define LOADSEC_CMD        18

#define LOADBIN_CMD        19
#define LOADPROFILE_CMD    20

#define VOLNAME_CMD        21
#define QUIT_CMD           22
#define ENULL_CMD          23


#ifndef MIN
  #define MIN(x,y) ( (x)<(y) ? (x):(y) )
#endif

#ifndef MAX
  #define MAX(x,y) ( (x)>(y) ? (x):(y) )
#endif


// Prototypes and Macros //////////////////////////////////////////////////////////////////////



// careful, cannot use ++/-- as parameters for TAGFILEID macro.
#define TAGFILEID(xmysect)  (tagfileid(F,xmysect))
#define TAGFFILEID(xmysect) (tagfileid(&F,xmysect))


void getcommand(void);
int floppy_disk_copy_image_open(DC42ImageType *F);
void hexprint(FILE *out, char *x, int size, int ascii_print);
void printsectheader(FILE *out, DC42ImageType *F, uint32 sector);
void printtag(FILE *out,DC42ImageType *F, uint32 sector);
void printsector(FILE *out,DC42ImageType *F, uint32 sector,uint32 sectorsize);
void cli(DC42ImageType *F);
void filenamecleanup(char *in, char *out);


long interleave5(long sector)
{
  static const int offset[]={0,5,10,15,4,9,14,3,8,13,2,7,12,1,6,11,16,21,26,31,20,25,30,19,24,29,18,23,28,17,22,27};
  return offset[sector%32] + sector-(sector%32);
}

long deinterleave5(long sector)
{
  static const int offset[]={0,13,10,7,4,1,14,11,8,5,2,15,12,9,6,3,16,29,26,23,20,17,30,27,24,21,18,31,28,25,22,19};
  return offset[sector%32] + sector-(sector%32);
}



char niceascii(char c)
{ c &=127;
 if (c<31) c|=32;
 if (c==127) c='.';
 return c;
}


uint8 hex2nibble(uint8 n)
{

 if (n>='0' && n<='9') return (uint8)(n-'0');
 if (n>='a' && n<='f') return (uint8)(n-'a'+10);
 if (n>='A' && n<='F') return (uint8)(n-'A'+10);
 return 255;
}

uint8 hex2byte(uint8 m, uint8 l)
{
// printf("Got:%c%c Returning:%02x:\n",m,l,(hex2nibble(m)<<4) | (hex2nibble(l)) );
 return (hex2nibble(m)<<4) | (hex2nibble(l));
}


uint32 hex2long(uint8 *s)
{
 char buffer[12];

 buffer[0]='0';
 buffer[1]='x';
 memcpy(&buffer[2],s,8);
 buffer[10]=0;

 //printf("Transforming:%s into %08x\n",buffer,strtol(buffer,NULL,16));
 return strtol(buffer,NULL,16);
}



uint16 tagfileid(DC42ImageType *F,int mysect)
{   uint8 *tag=dc42_read_sector_tags(F,mysect);
           if (!tag) return 0;
           return ((tag[4]<<8) | (tag[5]));
}

uint32 tagpair(DC42ImageType *F,int mysect, int offset)
{   uint8 *tag=dc42_read_sector_tags(F,mysect);


       // floppy?
       if (F->tagsize<20)  return ((tag[offset]<<8) | (tag[offset+1]));

       // profile/widget
       return ((tag[offset]<<16) |(tag[offset+1]<<8) | (tag[offset+2]));
}


int tagcmp(const void *p1, const void *p2)
{uint32 fileid1, fileid2,  abs1, abs2, next1, next2, prev1, prev2;

     // turn voids into sort tag types
     int i = ( *(int *)p2 );
     int j = ( *(int *)p1 );



     fileid1=tagfileid(&F,i);       // sadly we have to use the global variable here since qsort won't let us pass extra args
     fileid2=tagfileid(&F,j);


     if (F.tagsize<20)             // floppy?
     {
      abs1= tagpair(&F,i,6)  & 0x7ff;
      abs2= tagpair(&F,j,6)  & 0x7ff;

      next1=tagpair(&F,i,8)  & 0x7ff;
      next2=tagpair(&F,j,8)  & 0x7ff;

      prev1=tagpair(&F,i,10) & 0x7ff;
      prev2=tagpair(&F,j,10) & 0x7ff;
     }
     else                          // hard drive
     {                        //012345
      abs1= tagpair(&F,i,8)  & 0x7fffff;
      abs2= tagpair(&F,j,8)  & 0x7fffff;

      next1=tagpair(&F,i,0x0e)  & 0x7fffff;
      next2=tagpair(&F,j,0x0e)  & 0x7fffff;

      prev1=tagpair(&F,i,0x12) & 0x7fffff;
      prev2=tagpair(&F,j,0x12) & 0x7fffff;
     }

     // sort keys in order: file id, absolute sector #, next sector #


     if (fileid1>fileid2) return -1;
     if (fileid1<fileid2) return +1;

     if (abs1>abs2)       return -1;
     if (abs1<abs2)       return +1;

     if (next1>next2)     return -1;
     if (next1<next2)     return +1;

                          return  0; // if we made it here, they're equal, uh? problem possibly.
}


// Dump the data in the MDDF - Thanks to Chris McFall for this info.  :)
void dump_mddf(FILE *out, DC42ImageType *F)
{
 char *sec;
 int i,j ;
 uint32 sector, sect;

 if (!volumename[0])                        // if we already did the work don't bother.
  {//fprintf(out,"Searching for MDDF block.\n");
   for (sector=0; sector<F->numblocks; sector++)
     {
       sect=sorttag[sector];


       sec=(char *)dc42_read_sector_data(F,sect); //&(sectors[sect*sectorsize]);

       if (TAGFILEID(sect)==TAG_MDDF)
       {
        for (j=0,i=0x0d; i<0x20; i++,j++) volumename[j]=sec[i];

        fsversion=(sec[0]<<8)|sec[1];

        if (sect<firstmddf) firstmddf=sect;  // keep track of the first one in the image for bitmap sizing.

        fprintf(out,"MDDF (Superblock) found at sector #%04x(%4d) fsversion:%02x\n",sect,sect,fsversion);
       }
     }
     if (!fsversion) fprintf(out,"MDDF not found.\n");
  }
 fprintf(out,"\n-----------------------------------------------------------------------------\n");
 fprintf(out,"MDDF Volume Name: \"%s\"\n",volumename);
 switch(fsversion)
 {
  case 0x00 : fprintf(out,"FS Version information not found or MDDF version=0 or bad MMDF!\n"); break;
  case 0x0e : fprintf(out,"Version 0x0e: Flat File System with simple table catalog Release 1.0\n"); break;
  case 0x0f : fprintf(out,"Version 0x0f: Flat File System with hash table catalog Release 2.0 - Pepsi\n"); break;
  case 0x11 : fprintf(out,"Version 0x11: Hierarchial FS with B-Tree catalog Spring Release - 7/7\n"); break;

  default   : fprintf(out,"Unknow MDDF Version: %02x Could be we didn't find the right MDDF\n",version);
 }
 fprintf(out,"-----------------------------------------------------------------------------\n");
}

void get_allocation_bitmap(DC42ImageType *F)
{
 uint32 sector, i;
 unsigned int as=RESERVED_BLOCKS    ;   // sector is the search for the allocation bitmap, as is the active sector
 uint16 thistagfileid;
 char *sec;

 if (!allocated) allocated=malloc(F->numblocks*sizeof(int));

 // reserve all sectors initially as used. the code below will free them
 for (i=0; i<F->numblocks; i++) allocated[i]=9;

 // find and process all allocation bitmap block(s)
 //printf("Searching for block allocation bitmap blocks...\n");
 for (sector=0; sector<F->numblocks; sector++)
 {
   if (tagfileid(F,sector)==TAG_FREEBITMAP)                  // is this a bitmap block?
   {
     printf("Found allocation bitmap block at sector #%04x(%d)\n",sector,sector);
     sec=(char *)dc42_read_sector_data(F,sector);
     // do all the bits in this block until we go over the # of max sectors.
     for (i=0; i<F->datasize; i++)              // ??? might need to change this to match offsets ????
     {
      // This checks two things: one is the bitmap allocation bit corresponding to the active sector, and sets
      // it to 8 if it's on.  Then it checks the file id, if the file id=0, it'll set it to 1.  So if the
      // theory that the file id=0000 when the block is free holds true, the whole of allocated should
      // consists of only 0's and 9's.

      if (as>F->numblocks) break;       // the allocation bitmap is much larger than can fit inside a single sector.
                                        // so we have to ignore the rest of the bits therein.
                                        // (since 1 sector=512 bytes, 512*(8 bits/byte)=4096 allocated bits are available
                                        // but only 800 sectors in a 400k floppy, 1600 in an 800k, 2888 in a 1.4MB.
                                        // So we need to bail out before we overrun the allocated array.

      // Figure out which sectors are allocated or free.  We do this by both
      // checking the allocation bitmap block, *AND* the tags so that we can
      // detect errors in our code/assumptions.  For example, because of this,
      // I've discovered the posibility that 0x7fff is the tag file ID for
      // an erased block.

      // Macros to make the below block simpler for humans to read. :)
      #define USEDTAG (thistagfileid!=TAG_ERASED_BLK && thistagfileid!=TAG_FREE_BLOCK)
      #define ALLOC allocated[as]

      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &   1) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &   2) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &   4) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &   8) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &  16) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &  32) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] &  64) ? 8:0) | USEDTAG); as++;
      thistagfileid=TAGFILEID(as); ALLOC=(((sec[i] & 128) ? 8:0) | USEDTAG); as++;

      #undef USEDTAG
      #undef ALLOC
     }//for sector size
   }
 }
}

// return the type of file we are dealing with.
char *getfileid(int sector)
{
  uint32 fileid;
  static char fileidtext[64];            // needs to be static so it doesn't fall off the heap when this fn exits

  fileid=TAGFFILEID(sector);//((tags[(tagsize *(sector))+4] & 0xff)<<8)|((tags[(tagsize *(sector))+5]) & 0xff);

  snprintf(fileidtext,63,"file-%04x",fileid);

  if (fileid >TAG_MAXTAG    ) snprintf(fileidtext,63,"UnKnowN-%04x",fileid);
  if (fileid==TAG_ERASED_BLK) snprintf(fileidtext,63,"deleted-blocks-7fff");
  if (fileid==TAG_BOOTSECTOR) snprintf(fileidtext,63,"bootsect-aaaa");
  if (fileid==TAG_OS_LOADER ) snprintf(fileidtext,63,"OSLoader-bbbb");
  if (fileid==TAG_FREE_BLOCK) snprintf(fileidtext,63,"freeblocks-0000");
  if (fileid==TAG_MDDF      ) snprintf(fileidtext,63,"MDDF-0001");
  if (fileid==TAG_FREEBITMAP) snprintf(fileidtext,63,"alloc-bitmap-0002");
  if (fileid==TAG_S_RECORDS ) snprintf(fileidtext,63,"srecords-0003");
  if (fileid==TAG_DIRECTORY ) snprintf(fileidtext,63,"directory-0004");
  return fileidtext;
}

char *getfileidbyid(uint16 fileid)
{
  static char fileidtext[64];            // needs to be static so it doesn't fall off the heap when this fn exits
  snprintf(fileidtext,63,"file-%04x",fileid);

  if (fileid >TAG_MAXTAG    ) snprintf(fileidtext,63,"unknown-%04x",fileid);  // catch other unknown file id's
  if (fileid==TAG_BOOTSECTOR) snprintf(fileidtext,63,"bootsect-aaaa");
  if (fileid==TAG_OS_LOADER ) snprintf(fileidtext,63,"OSLoader-bbbb");
  if (fileid==TAG_FREE_BLOCK) snprintf(fileidtext,63,"freeblocks-0000");
  if (fileid==TAG_MDDF      ) snprintf(fileidtext,63,"MDDF-0001");
  if (fileid==TAG_FREEBITMAP) snprintf(fileidtext,63,"alloc-bitmap-0002");
  if (fileid==TAG_S_RECORDS ) snprintf(fileidtext,63,"srecords-0003");
  if (fileid==TAG_DIRECTORY ) snprintf(fileidtext,63,"directory-0004");
  return fileidtext;
}



void get_dir_file_names(DC42ImageType *F)
{
 uint32 sector, mysect, i,j,k,ti,ts;       // sector is the search for the allocation bitmap, as is the active sector
 uint16 fileid;
 char *sec, *f;

 //int offset=0x10;                               // start offset

 char filename[128];                            // current file name I'm working on

 for (i=0; i<65536; i++)                        // set default file names
  {
   f=getfileidbyid(i);
   strncpy(filenames[i][0],f,63);
   filenamecleanup(filenames[i][0],filenames[i][1]);
  }

 // Walk all (sorted) sectors until we find the directory entries.
 i=0x10;                                                       // initial filename position for 1st dir block
 for (sector=0; sector<F->numblocks; sector++)
 {
   mysect=sorttag[sector];

   if (TAGFILEID(mysect)==TAG_DIRECTORY)     // is this a directory block? /////////////////////////////////////////
   {                                         // Yes? Walk it for filenames and extract tag id's.
     //printf("Searching directory at sector %d\n",mysect);
     //sec=&(sectors[mysect*sectorsize]);
     sec=(char *)dc42_read_sector_data(F,mysect);
     while(i+=64)       // every 64 bytes is a directory entry.
     {
       if (i>511)                                              // check to see if we went over sector boundary
       {  sector++; i-=512;                                    // advance to the next sector, fix index.
          mysect=sorttag[sector];                           // get the sorted sector
          //sec= &(sectors[mysect*sectorsize]);            // get next data pointer
          sec=(char *)dc42_read_sector_data(F,mysect);
          if (TAGFILEID(mysect)!=TAG_DIRECTORY) return;        // done, we've gone out of the last dir.
          //printf("Searching directory at sector %d\n",mysect);
       }

       if (sec[i]==0 && sec[i+1]>31)                           // do we have a null in [0] and text byte in [1]?
       {                                                       // file name starts on the 1st byte, not the zeroth byte.
         for (j=0,k=i+1;j<32; j++,k++) filename[j]=sec[k];     // copy the file name
         filename[31]=0;                                       // make sure it's terminated
         // get the file ID for this sector.
         fileid=(sec[i+32+4]<<8)|sec[i+32+5];                  // get the tag for this file



         // If the file id tag isn't a reserved fileid type *AND*
         // it exists in the sectors, then we likely have a real
         // file name, so save it.

         if (fileid!=TAG_BOOTSECTOR && fileid!=TAG_OS_LOADER && fileid!=TAG_FREE_BLOCK &&
             fileid!=TAG_ERASED_BLK && fileid!=TAG_MDDF      && fileid!=TAG_FREEBITMAP &&
             fileid!=TAG_S_RECORDS  && fileid!=TAG_DIRECTORY                              )
            for (ti=0; ti<F->numblocks; ti++)
            {
             ts=sorttag[ti];
             if (TAGFILEID(ts)==fileid)
              {// associate the file name with the tag file id.
               char temp[1024];

               strncpy(filenames[fileid][0],filename,63);      // add Lisa File name to generic file id name
               filenamecleanup(filenames[fileid][0],           // clean it up, now output name is set
                               filenames[fileid][1]);
               //strncpy(filenames[fileid][0],filename,31);    // copy lisa name over generic name
               //printf("Found file name:%s for file tag id:%04x in sector %04x(%d) at offset:%04x(%d)\n",filename,fileid,mysect,mysect,i,i);
               snprintf(temp,1023,"%04x     %-32s    %04x(%d)\n",fileid,filename,ts,ts);
               strncat(directory,temp,65535);                  // add filename to directory.
               break;                                          // quit searching
              }
            }

       } // else {printf("No filename at offset %04x(%d) in sector %04x(%d)\n",i,i,mysect,mysect);}


     }///for sector size /////////////////////////////////////////////////////////////////////////////////////////
   }
   else if ( TAGFILEID(mysect)>TAG_DIRECTORY) break;  // don't bother looking for more dirs, these are sorted.
 } // for sector loop ///////////////////////////////////////////////////////////////////////////////////////////


}


// Sort the tags by the keys above, then extract information that's tag sensitive.
void sorttags(DC42ImageType *F)
{
 unsigned int i;

 if (!sorttag) sorttag=malloc(F->numblocks*sizeof(int));

 for (i=0; i<F->numblocks; i++) sorttag[i]=i;  // initial index of tags is a 1:1 mapping
 qsort((void *)sorttag, (size_t) F->numblocks, sizeof (int), tagcmp); // sort'em

 get_allocation_bitmap(F);
 get_dir_file_names(F);
 dump_mddf(stdout,F);
}



void dump_allocation_bitmap(FILE *out, DC42ImageType *F)
{
 int i;
 unsigned int j;
 char c;
 fprintf(out,"\n                  Block allocation bitmap\n");
 fprintf(out,"----------+----------------------------------------+\n");
 fprintf(out,"Sector    |..........1.........2........3..........|\n");
 fprintf(out,"hex dec  +|0123456789012345678901234567890123456789|\n");
 fprintf(out,"----------+----------------------------------------|\n");

 for (j=0; j<F->numblocks; j+=40)
 {
  fprintf(out,"%04x(%04d)|",j,j);
  for (i=0; i<40; i++)
       {
        switch(allocated[i+j])
        {
          case 0:  c=' '; break;
          case 9:  c='#'; break;
          case 1:  c='.'; break;
          case 8:  c='8'; break;
          default: c='?'; break;
        }
        fprintf(out,"%c", c);
       }
  fprintf(out,"|\n");
 }
 fprintf(out,"----------+----------------------------------------+\n\n");
}


void filenamecleanup(char *in, char *out)
{
 uint32 j;
 char c;
      for (j=0; in[j] && j<63; j++)     // santize file name for most host OS's.
      {                                 // some of these are over cautious, i.e. spaces can be escaped in unix, etc...
                                        // but I want to make sure we don't cause needless trouble and require escaping
                                        // with lots of backslashes, etc.  In fact, you can even build file names containing
                                        // wildcards and slashes in unix, but accessing them from the commandline becomes
                                        // an excercise in backslash insanity
        c=in[j];
        switch(c)
          {
           case '"' :               // get rid of quotes
           case '`' :
           case '\'':
           case '?' :               // wild cards
           case '*' :
           case '/' : // slashes
           case '\\': c='-'; break;
           case ':' : c='-'; break; // colons for MSFT OS's

           case '!' :               // !'s for unix
           case '&' :               // this means background in most unix shells.
           case '$' :               // shell environment variables for unix + windows
           case '%' :

           case '<' :               // redirects and pipe
           case '>' :
           case '|' :
           case '(' :
           case ')' :
           case '[' :
           case ']' :
                      c='_';
          }
        if (c<33 || c>126) c='_';     // control and high chars

        out[j]=c;
      }
      out[63]=0; // make double dog sure the string is terminated;
}

// Walk the directory catalog and read all of the file names stored in there
// then store them in the table.


void extract_files(DC42ImageType *F)
{
  /*-----------------2/15/2004 6:57PM-----------------
   * extract all files inside of a disk image
   *
   * This could use better error handling, and also needs
   * to extract the friendly+system file names.  For now
   * like all things, it's a damned hack. ;)
   * --------------------------------------------------*/

  FILE *fh=NULL, *fb=NULL, *fx=NULL;    // file handles for header, binary, and hex
  unsigned int sector=0;                // sector number
  char newfile[FILENAME_MAX];           // name of file or name of header metadata
  char newfilex[FILENAME_MAX];          // name of hex/text file to create
  char newfileb[FILENAME_MAX];          // name of binary file to create
  char newfilemb[FILENAME_MAX];         // name of meta data binary file to create
  char newfilemh[FILENAME_MAX];         // name of meta data hex binary file to create

  char newdir[1024];                    // name of new directory to create
  char *sub;                            // substring search to chop extension
  char *sec;                            // pointer to sector data

  int chop0xf0=0;                       // type of file ID, and whether there's a metadata attached.
  uint16 fileid, oldfileid=0xffff;
  int sect;                             // translated sector number
  int err;

  // create a directory with the same name as the image in the current directory and enter it ////////
   snprintf(newfile,FILENAME_MAX,F->fname);

  // whack off any extensions           // Butthead: "Huh huh, huh huh, he said 'Whack!'"
                                        // Beavis:   "Yeah, Yeah! Then he said 'Extensions'"
                                        // Butthead: "Huh huh, huh huh, that was cool!"

  sub=strstr(newfile,".dc42");  if (sub) *sub=0;
  sub=strstr(newfile,".DC42");  if (sub) *sub=0;
  sub=strstr(newfile,".dc");    if (sub) *sub=0;
  sub=strstr(newfile,".DC");    if (sub) *sub=0;
  sub=strstr(newfile,".image"); if (sub) *sub=0;
  sub=strstr(newfile,".Image"); if (sub) *sub=0;
  sub=strstr(newfile,".img");   if (sub) *sub=0;
  sub=strstr(newfile,".IMG");   if (sub) *sub=0;
  sub=strstr(newfile,".dmg");   if (sub) *sub=0;
  sub=strstr(newfile,".DMG");   if (sub) *sub=0;



  // add a .d to the file name, then create and enter the directory
  snprintf(newdir,1023,"%s.d",newfile);
  printf("Creating directory %s to store extracted files\n",newdir);

#ifndef __MSVCRT__
  err=mkdir(newdir,00755);
#else
  err=mkdir(newdir);
#endif

  // Might want to change this to ask if it's ok to overwrite this directory.
  if (err && errno!=EEXIST)  {fprintf(stderr,"Couldn't create directory %s because (%d)",newdir,err);
                              perror("\n"); return;}
  if (chdir(newdir))         {fprintf(stderr,"Couldn't enter directory %s because",newdir);
                              perror("\n"); return;}

  // Extract the entire disk //////////////////////////////////////////////////////////////////////////


  errno=0;


  // extract files -----------------------------------------------------------------------
  for (sector=0; sector<F->numblocks; sector++)
       {
        sect=sorttag[sector];        // dump files from sectors in sorted order
        fileid=TAGFILEID(sect);
        sec=(char *)dc42_read_sector_data(F,sect);

        if (fileid!=oldfileid)          // we have a file id we haven't seen before. open new file handles
        {
          fflush(stdout); fflush(stderr);
          if (fx) {fflush(fx); fclose(fx); fx=NULL;}
          if (fb) {fflush(fb); fclose(fb); fb=NULL;}
          if (fx) {fflush(fh); fclose(fh); fh=NULL;}

          printf("%04x!=%04x",fileid,oldfileid);


          if (strlen(filenames[fileid][1])) snprintf(newfile,63,"%s",   filenames[fileid][1]);
          else                              snprintf(newfile,63,"%s",   getfileid(sect)                     );
          printf("Extracting: %s -> %s (.bin/.txt)\n",filenames[fileid][0],newfile);

          chop0xf0=1;                 // default for normal files is to have a header
                                      // but special files do not, so catch those
                                      // and don't chop the first 0xf0 bytes

          if (fileid==TAG_BOOTSECTOR) chop0xf0=0;  // boot sector
          if (fileid==TAG_OS_LOADER ) chop0xf0=0;  // os loader
          if (fileid==TAG_ERASED_BLK) chop0xf0=0;  // deleted blocks
          if (fileid==TAG_FREE_BLOCK) chop0xf0=0;  // free blocks
          if (fileid==TAG_MDDF      ) chop0xf0=0;  // mddf
          if (fileid==TAG_FREEBITMAP) chop0xf0=0;  // allocation bitmap
          if (fileid==TAG_S_RECORDS ) chop0xf0=0;  // s-records
          if (fileid==TAG_DIRECTORY ) chop0xf0=0;  // directory
          if (fileid >TAG_MAXTAG    ) chop0xf0=0;  // catch other unknown file id's

          snprintf(newfilex,1024,"%s.txt",newfile);
          fx=fopen(newfilex,"wt");
          if (!fx)                  {fprintf(stderr,"Couldn't create file %s\n",newfilex);
                                     perror("\n"); chdir(".."); return;}

          snprintf(newfileb,1024,"%s.bin",newfile);
          fb=fopen(newfileb,"wb");
          if (!fb)                  {fprintf(stderr,"Couldn't create file %s\n",newfileb);
                                     perror("\n"); chdir(".."); return;}

          //printf("File:%s starts at sector %d %s metadata\n",newfile, sect, chop0xf0 ? "has":"has no ");

          if (chop0xf0)                 // deal with meta data bearing files----------------------
          {
              // We need to chop off the 1st 0xf0 bytes as they're part of the metadata of the file.
              // create two metadata files, one binary, and one hex.
              snprintf(newfilemb,1024,"%s.meta.bin",newfile);
              fh=fopen(newfilemb,"wb");
              if (!fh)                  {fprintf(stderr,"Couldn't create file %s\n",newfilemb);
                                         perror("\n"); chdir(".."); return;}
              //sec=&(sectors[sect*sectorsize]);
              //sec=(char *)dc42_read_sector_data(F,sect);
              fwrite(sec,0xf0,1,fh); fclose(fh); fh=NULL;
              if (errno) {fprintf(stderr,"An error occured on file %s",newfile); perror("");
                         fclose(fb); fclose(fx); chdir(".."); return;}

              //write remainder of sector to the binary file
              //sec=&(sectors[sect*sectorsize+0xf0]);
              sec=(char *)dc42_read_sector_data(F,sect);
              //fwrite(sec,(F->datasize-0xf0),1,fb);     // bug found by Rebecca Bettencourt
              fwrite(sec+0xf0,(F->datasize-0xf0),1,fb);  // bug found by Rebecca Bettencourt
              if (errno) {fprintf(stderr,"An error occured on file %s",newfileb); perror("");
                         fclose(fb); fclose(fx); chdir(".."); return;}


              snprintf(newfilemh,1024,"%s.meta.txt",newfile);
              fh=fopen(newfilemh,"wb");
              if (!fh)                  {fprintf(stderr,"Couldn't create file %s\n",newfilemh);
                                         fclose(fb); fclose(fx); perror("\n"); chdir(".."); return;}
              printsector(fh,F,sect,0xf0); fclose(fh); fh=NULL;
              if (errno) {fprintf(stderr,"An error occured on file %s",newfileb); perror("");
                         fclose(fb); fclose(fx); chdir(".."); return;}

              // dump the data to the hex file, but add a banner warning about metadata.
              fprintf(fx,"\n\n[Metadata from bytes 0x0000-0x00ef]\n");
              printsector(fx,F,sect,F->datasize);

              oldfileid=fileid;             // set up for next round
              continue;                 // skip to the next sector, this one is done ----------------
          } // end of chop0xf0 ----------------------------------------------------------------------

        } // end of new file comparison on oldfileid/fileid  ----------------------------------------


        // now, write the sector out in hex and binary. ---------------------------------------------
        //printf("Writing sector %04x(%d) to %s\n",sect,sect,newfile);
        //void printsector(FILE *out,DC42ImageType *F, uint32 sector,uint32 sectorsize);

        printsector(fx,F,sect,F->datasize);

        if (errno) {fprintf(stderr,"An error occured on file %s.txt",newfile); perror("");
                   fclose(fb); fclose(fx); chdir(".."); return;}

        fwrite(sec,F->datasize,1,fb);
        if (errno) {fprintf(stderr,"An error occured on file %s.bin",newfile); perror("");
                   fclose(fb); fclose(fx); chdir(".."); return;}

        oldfileid=fileid;             // set up for next round

       } // end of all sectors. ---------------------------------------------------------------------

       // cleanup and return;
       if (fx) {fflush(fx); fclose(fx); fx=NULL;}
       if (fb) {fflush(fb); fclose(fb); fb=NULL;}
       if (fx) {fflush(fh); fclose(fh); fh=NULL;}
       chdir("..");
       return;
}




void getcommand(void)
{
char *space, *s;
int i;
long len,l;
char line[8192];
char *cur=cargsstorage;

// clear arguements
for ( i=0; i<MAXARGS; i++) cargs[i]=NULL;
memset(cargsstorage,0,8192);

command=LASTCOMMAND;
fgets(line,8192,stdin);

strncpy(cmd_line,line,8192);
cmd_line[strlen(cmd_line)-1]=0;

len=strlen(line);
if (feof(stdin)) {puts(""); exit(1);}
if (len) line[--len]=0; else return; // knock out eol char
if (!len) {command=-1;return;}       // shortcut for next sector.
if (line[0]=='!')                 {if (len==1) system("sh");
                                   else        system(&line[1]);
                                   command=NULL_CMD;}                                       // shell out
if (line[0]=='?')                 {strncpy(line,"help",6);}                                 // help synonym
if (line[0]=='+' && len==1)       {command=SECTOR_NXT; return;}                             // next sector
if (line[0]=='-' && len==1)       {command=SECTOR_PRV; return;}                             // previous sector
if (line[0]=='+' || line[0]=='-') {l=strtol(line,NULL,0);sector+=l; command=DISPLAY_CMD; return;} // delta jump

space=strchr(line,32);
if (space) space[0]=0;

for (i=0; i<LASTCOMMAND; i++) if (strncmp(line,cmdstrings[i],16)==0) command=i;
// shortcut for sector number
iargs[0]=strtol(line,NULL,0); if ( line[0]>='0' && line[0]<='9' ) {command=0; return;}
if (command==LASTCOMMAND) {puts("Say what?  Type in help for help..\n"); return;}
if (command==QUITCOMMAND) {puts("Closing image"); dc42_close_image(&F); puts("Bye"); exit(0);}
if (!space) return;
line[len]=' ';
line[len+1]=0;

s=space+1;

lastarg=0;
while (  (space=strchr(s,(int)' '))!=NULL && lastarg<MAXARGS)
 {
  *space=0;
  cargs[lastarg]=cur;
  strncpy(cargs[lastarg],s,255);
  cur+=strlen(cargs[lastarg])+1;

  iargs[lastarg]=strtol(s, NULL, 0);
  hargs[lastarg]=strtol(s, NULL, 16);
  dargs[lastarg]=strtol(s, NULL, 10);
  s=space+1;

  lastarg++;
 }

}



//int floppy_disk_copy_image_open(DC42ImageType *F)
//{
//
//return dc42_open(DC42ImageType *F, char *filename, char *options);
//
//}

/*
uint32 i,j;
unsigned char comment[64];
unsigned char dc42head[8192];
uint32 datasize=0, tagsize=0, datachks=0, tagchks=0, mydatachks=0L, mytagchks=0L;
uint16 diskformat=0, formatbyte=0, privflag=0;

    errno=0;
    fseek(F->fhandle, 0,0);
    fread(dc42head,84,1,F->fhandle);
    if (errno) {perror("Got an error."); exit(1);}

    memcpy(comment,&dc42head[1],64);
    comment[63]=0;
    if (dc42head[0]>63) {fprintf(stderr,"Warning pascal str length of label is %d bytes!\n",(int) (dc42head[0]));}
    else comment[dc42head[0]]=0;

    F->sectoroffset=84;
    datasize=(dc42head[64+0]<<24)|(dc42head[64+1]<<16)|(dc42head[64+2]<<8)|dc42head[64+3];
    tagsize =(dc42head[68+0]<<24)|(dc42head[68+1]<<16)|(dc42head[68+2]<<8)|dc42head[68+3];
    datachks=(dc42head[72+0]<<24)|(dc42head[72+1]<<16)|(dc42head[72+2]<<8)|dc42head[72+3];
    tagchks =(dc42head[76+0]<<24)|(dc42head[76+1]<<16)|(dc42head[76+2]<<8)|dc42head[76+3];

    tagstart=84+datasize;

    diskformat=dc42head[80];
    formatbyte=dc42head[81];
    privflag=(dc42head[82]<<8 | dc42head[83]);

    printf("Header comment :\"%s\"\n",comment);
    printf("Data Size      :%ld (0x%08x)\n",datasize,datasize);
    printf("Tag total      :%ld (0x%08x)\n",tagsize,tagsize);
    printf("Data checksum  :%ld (0x%08x)\n",datachks,datachks);
    printf("Tag checksum   :%ld (0x%08x)\n",tagchks,tagchks);
    printf("Disk format    :%d  ",diskformat);

    switch(diskformat)
    {
        case 0: printf("400K GCR\n"); break;
        case 1: printf("800K GCR\n"); break;
        case 2: printf("720K MFM\n"); break;
        case 3: printf("1440K MFM\n"); break;
        default: printf("unknown\n");
    }
    printf("Format byte    :0x%02x   ",formatbyte);
    switch(formatbyte)
    {
        case 0x12: printf("400K\n"); break;
        case 0x22: printf(">400k\n"); break;
        case 0x24: printf("800k Apple II Disk\n"); break;
        default: printf("unknown\n");
    }
    printf("Private        :0x%04x (should be 0x100)\n",privflag);
    printf("Data starts at :0x%04x (%ld)\n",84,84);
    printf("Tags start at  :0x%04x (%ld)\n",tagstart,tagstart);
    sectorsize=512;
    F->numblocks=datasize/sectorsize;
    tagstart=84 + datasize;
    tagsize=tagsize/F->numblocks;

    F->numblocks=datasize/sectorsize;        // turn this back into 512 bytes.

    if (F->numblocks==800)
    {
        F->maxtrk=80; F->maxsec=13;F->maxside=0;
        F->ftype=1;
    }
    if (F->numblocks==1600)
    {
        F->maxtrk=80; F->maxsec=13 ;F->maxside=1;
        F->ftype=2;
    }
    printf("No of sectors  :0x%04x (%d)\n",F->numblocks,F->numblocks);
    printf("Sector size    :0x%04x (%d)\n",sectorsize,sectorsize);
    printf("tag size       :0x%04x (%d)\n",tagsize,tagsize);

    tagsize=12; // force it for now.

    //printf("Allocating %d bytes (%d blocks * %d sectorsize)",4+F->numblocks*sectorsize,F->numblocks,sectorsize);
    (uint8 *)sectors=malloc(4+ F->numblocks * sectorsize) ; if ( !sectors) {printf("- failed!\n"); return 1;}
    puts("");

    //printf("Allocating %d tag bytes (%d blocks * %d tagsize)",4+F->numblocks*tagsize,F->numblocks,tagsize);
    (uint8 *)tags=        malloc(4+ F->numblocks * tagsize);     if ( !tags ) {printf(" - failed!\n"); return 1;}

    //printf("Allocating %d bytes for free bitmap (%d blocks)",4+F->numblocks*tagsize,F->numblocks);
    (uint8 *)allocated=   malloc(4+ F->numblocks             );     if ( !allocated) {printf(" - failed!\n"); return 1;}

    //puts("");
    memset(sectors,  0,( F->numblocks * sectorsize) );
    memset(tags,     0,( F->numblocks * tagsize   ) );
    memset(allocated,0,( F->numblocks                ) );

    fflush(stdout);
    fflush(stdout);

        // do it in one shot
    fseek(F->fhandle,84,0);
    //fread((char *) sectors,sectorsize*F->numblocks,1,F->fhandle);
    fread((char *) sectors,sectorsize,F->numblocks,F->fhandle);
    if (errno) {perror("Got an error."); exit(1);}

    mydatachks=dc42_get_data_checksum(F);

    //fseek(F->fhandle, i *(tagsize)+(tagstart),0);
    //fread((char *) tags,tagsize*F->numblocks,1,F->fhandle);
    fread((uint8 *) tags, tagsize, F->numblocks, F->fhandle);
    if (errno) {perror("Got an error whilst attempting to read tags."); exit(1);}

    mytagchks=dc42_get_tag_checksum(F);

    printf("Header/Calc data chksum   0x%08x / 0x%08x diff:%ld\n",datachks,mydatachks,mydatachks-mydatachks);
    printf("Header/Calc tag chksum    0x%08x / 0x%08x diff:%ld\n",tagchks,mytagchks,  mydatachks-datachks  );

    puts("");

    havetags=dc42_has_tags(F);
    if (!havetags)
            {
                puts("\n***** Looks like all tag data is null. You will not be able to do much with");
                puts("this Disk Image!  See the documentation for more information.");
            }


    errno=0;
    sorttag=(int *) malloc(F->numblocks * sizeof(int) +2);
    if (!sorttag) {perror("Couldn't allocate space for sortted tag index array\n"); exit(2);}

    return 0;
}
*/


void hexprint(FILE *out, char *x, int size, int ascii_print)
{
int i,half;
unsigned char c;
char ascii[130];
half=(size/2) -1;
   if (size>128) {fprintf(stderr,"hexprintf given illegal size %d\n",size); exit(1);};
   memset(ascii,0,130);
   for (i=0; i<size; i++)
    {
     c=x[i];

     if (i==half) fprintf(out,"%02x . ",c);
     else fprintf(out,"%02x ",c);

     if (ascii_print)
          {
        if (c>126) c &=127;
        if (c<31)     c |= 32;
        ascii[i]=c;
      }
    }
   if (size<16) while(16-size) {fprintf(out,"   "); size++;}
   if (ascii_print) fprintf(out,"  |  %s\n",ascii);
}

void printsectheader(FILE *out,DC42ImageType *F, uint32 sector)
{
 uint32 fileid;
 fprintf(out,"\n-----------------------------------------------------------------------------\n");
 //fprintf(out,"Sec %d:(0x%04x) Cluster:%d, csize:%d ",sector,sector,sector/clustersize,clustersize);


 fprintf(out,"Sec %d:(0x%04x)  ",sector,sector);

 if (havetags)
 {    if (tagsaresorted)                     // when tags are sorted, allocation bitmap is extracted.
      fprintf(out," %s Block ",
      ((allocated[sector] & 8) ? "Used":"Free"));
      fileid=TAGFILEID(sector);
      fprintf(out,"Part of file %s:\"%s\"",getfileid(sector),filenames[fileid][0]);
 }

 fprintf(out,"\n-----------------------------------------------------------------------------\n");
if (F->tagsize)
{
if (F->tagsize==12)
 fprintf(out,"            +0 +1 +2 +3 +4 +5 . +6 +7 +8 +9 +a +b\n");
else
 fprintf(out,"            +0 +1 +2 +3 +4 +5 . +6 +7 +8 +9 +a +b +c +d +e +f+10+11+12+13\n");

 fprintf(out,"tags:       ");

 hexprint(out,  (char *)dc42_read_sector_tags(F,sector) ,F->tagsize,0);

if (F->tagsize==12)
  fprintf(out,"\n           |volid| ??  |fileid|absnext|next|previous\n");
else
  fprintf(out,"\n           |volid|????|fileid||absnext|   ?   |    ?   |    next|previous\n");
}
 //fprintf(out,"\n\n");
 fprintf(out,"-----------------------------------------------------------------------------\n");
 fprintf(out,"      +0 +1 +2 +3 +4 +5 +6 +7 . +8 +9 +a +b +c +d +e +f                    \n");
 fprintf(out,"-----------------------------------------------------------------------------\n");
}



void printtag(FILE *out,DC42ImageType *F, uint32 sector)
{
 char *s;

 switch (allocated[sector])
 {
     case 0:  s="free"; break;
     case 9:  s="used"; break;
     case 1:  s="ufid"; break;
     case 8:  s="ubit"; break;
     default: s="????"; break;
 }


 fprintf(out,"%4d(%04x): ", sector,sector);
 hexprint(out, (char *)dc42_read_sector_tags(F,sector) ,F->tagsize,0);
 fprintf(out," %s\n",s);
}


void printsector(FILE *out,DC42ImageType *F, uint32 sector,uint32 sectorsize)
{

 uint16 i;
 char *sec;

 //fprintf(stderr,"\n:sector#: %d sectorsize: %d\n",sector,sectorsize);

 if (sector>F->numblocks) {fprintf(out,"not that many sectors!\n"); sector=F->numblocks-1;}

 printsectheader(out,F,sector);
 //sec=&(sectors[sector*sectorsize]);
 sec=(char *)dc42_read_sector_data(F,sector);
 for (i=0; i<F->datasize; i+=dispsize)
     { fprintf(out,"%04x: ",i); hexprint(out,(char *)(&sec[i]),dispsize,1); }
 fprintf(out,"\n");

 //printf("\nResults:%d :%s pointer:%p\n",i,F->errormsg,F->RAM);
}


void version_banner(void)
{
  //   ..........1.........2.........3.........4.........5.........6.........7.........8
  //   012345678901234567890123456789012345678901234567890123456789012345678901234567890
  puts("  ---------------------------------------------------------------------------");
  puts("    Lisa File System Shell Tool  v0.98     http://lisaem.sunder.net/lisafsh  ");
  puts("  ---------------------------------------------------------------------------");
  puts("         Copyright (C) MMXII, Ray A. Arachelian, All Rights Reserved.");
  puts("              Released under the GNU Public License, Version 2.0");
  puts("    There is absolutely no warranty for this program. Use at your own risk.  ");
  puts("  ---------------------------------------------------------------------------\n");
}

void cli(DC42ImageType *F)
{
int i;
uint16 newsector=0;

while (1)
 {
   fflush(stderr); fflush(stdout);
   printf("lisafsh> ");

   getcommand();

   switch(command)
   {
       case  NULL_CMD :              break;
       case  SECTOR_NXT:             sector++; printsector(stdout,F,sector,F->datasize);break;
       case  SECTOR_PRV:             sector--; printsector(stdout,F,sector,F->datasize);break;
       case  SECTOR_CMD:             sector=iargs[0]; printsector(stdout,F,sector,F->datasize);break;
       case  CLUSTER_CMD:            sector=iargs[0]*clustersize/512; printsector(stdout,F,sector,F->datasize);break;
       case  DISPLAY_CMD:            printsector(stdout,F,sector,F->datasize); break;
       case  SETCLUSTERSIZE_CMD:     clustersize=iargs[0]; break;
       case  DUMP_CMD:               {for (sector=0; sector<F->numblocks; sector++) printsector(stdout,F,sector,F->datasize); } break;
       case  TAGDUMP_CMD:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           {
            puts("\n\n      +0 +1 +2 +3 +4 +5   +6 +7 +8 +9 +a +b");
             puts(  "-----------------------------------------------------------------------------");
            for (sector=0; sector<F->numblocks; sector++) printtag(stdout,F,sector);
           }
           break;
       case SORTTAGDUMP_CMD:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);   tagsaresorted=1;}
           puts("\n\n      +0 +1 +2 +3 +4 +5   +6 +7 +8 +9 +a +b");
           puts(    "-----------------------------------------------------------------------------");
           for (sector=0; sector<F->numblocks; sector++) printtag(stdout,F,sorttag[sector]);
           break;
       case SORTDUMP_CMD:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
           for (sector=0; sector<F->numblocks; sector++) printsector(stdout,F,sorttag[sector],F->datasize);
           break;
       case EXTRACT_CMD:
           // extract all Lisa files in disk based on the tags.
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
           extract_files(F);
           break;

       case BITMAP_CMD:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
           dump_allocation_bitmap(stdout,F); break;

       case SORT_NEXT:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
           {
             unsigned int i;
             for (i=0; i<F->numblocks-1; i++)
               if ((unsigned)sorttag[i]==sector)
                  {newsector=sorttag[i+1]; printsector(stdout,F,sector,F->datasize);break;}
           }

           if (newsector<=F->numblocks && newsector!=sector) sector=newsector;
           else puts("This sector is the end of this chain.");
           break;

       case SORT_PREV:
           if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
           if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
           {
               unsigned int i;
               for (i=1; i<=F->numblocks-1; i++)
               if  ((unsigned)sorttag[i]==sector)
                   {newsector=sorttag[i-1]; printsector(stdout,F,sector,F->datasize);break;}
           }

           if (newsector<=F->numblocks && newsector!=sector) sector=newsector;
           else puts("This sector is the start of this chain.");
           break;

       case DIR_CMD:
            if (!havetags) {puts("I can't do that, this image doesn't have tags."); break;}
            if (!tagsaresorted) {sorttags(F);    tagsaresorted=1;}
            //                 01234567890123456789012345678912
            printf("\nFileID   FileName                           start sector\n");
            printf("----------------------------------------------------------\n%s\n",directory);
            break;

       case  VERSION_CMD: version_banner(); break;


       case EDITSECTOR_CMD:
           {
            int offset=iargs[0];
            int i,j;
            uint8 mysector[2048];

            if (lastarg<3)
               {
                 puts("editsector {offset} {hexdata}");
                 puts("                - edit the current sector by writing the hex data at offset");
                 puts("                  the change is immediately written to the file.");
                 puts("             i.e. 'editsector 0x0F CA FE 01' writes bytes CA FE 01 offset 15");
                 break;
               }
               // read the sector into a buffer
               memcpy(mysector,dc42_read_sector_data(F, sector),F->datasize);

               // modify
               {
                unsigned int i;
                for (i=offset,j=1; i<F->datasize && cargs[j]!=NULL; i++,j++) mysector[i]=hargs[j];
               }
               // write
               i=dc42_write_sector_data(F, sector, mysector);
               printsector(stdout,F,sector,F->datasize);
               if (i) printf("Results:%d :%s\n",i,F->errormsg);
           }                 break;

       case EDITTAG_CMD:
           {
            int offset=iargs[0];
            int i,j;
            uint8 mytag[64];

            if (lastarg<3)
               {
                   puts("edittag    {offset} {hexdata}");
                   puts("                - edit the current sector's tags by writing the hex data at the");
                   puts("                  the offset.  the change is immediately written to the file.");
                   puts("             i.e. 'editsector 4 00 00' writes bytes 0000 at offset 4 ");
                   puts("                  (it sets the FILEID tag such that it marks the block as free)");

                 break;
               }
               // read the sector into a buffer
               memcpy(mytag,dc42_read_sector_tags(F, sector),F->tagsize);

               {// modify
               unsigned int i;
               for (i=offset,j=1; i<F->tagsize && cargs[j]; i++,j++) mytag[i]=hargs[j];
               }
               // write
               i=dc42_write_sector_tags(F, sector, mytag);
               if (i) printf("****WARNING, WRITE FAILED!****\n");

               printsector(stdout,F,sector,F->datasize);
           }
           break;
       case LOADSEC_CMD :
            {
             uint8 mysector[2048];
             FILE *binfile;
             uint32 offset=iargs[0];

             binfile=fopen(cargs[1],"rb");

             if (cargs[1]==NULL) {printf("Missing parameters\n"); break;}
             if (!binfile) {printf("Could not open file: %s to load sector at offset:%d (%x)\n",cargs[1],iargs[0]); break;}

             fseek(binfile,offset,SEEK_SET);
             fread(mysector,F->datasize,1,binfile);
             fclose(binfile);
             i=dc42_write_sector_data(F, sector, mysector);
             printsector(stdout,F,sector,F->datasize);
            }
            break;


       case LOADBIN_CMD:
            {
             uint8 mysector[2048];
             FILE *binfile;
             uint32 offset=iargs[0];
             uint32 osector=sector;

             errno=0;
             binfile=fopen(cargs[1],"rb");
             printf("Opening binary file: %s for loading into disk image.\n",cargs[1]);
             if (errno) perror("Could not open file.\n");

             if (cargs[1]==NULL) {printf("Missing parameters\n"); break;}
             if (!binfile) {printf("Could not open file: %s to load at offset:%d (%x)\n",cargs[1],iargs[0]); break;}

             fseek(binfile,offset,SEEK_SET);

             while( fread(mysector,F->datasize,1,binfile) && sector<F->numblocks && !feof(binfile) && !ferror(binfile))
                  {
                    i=dc42_write_sector_data(F, sector, mysector);                  // write the data tothe sector
                    printf("Loaded %d bytes into sector:%d from file offset:%ld\n",F->datasize,sector,offset);
                    sector++;                                                       // prepare next sector to load
                    offset=ftell(binfile);                                          // grab next offset addr so we can print it
                  }
                  fclose(binfile);


            sector=osector;
            }
            break;


       case LOADPROFILE_CMD:
            {
             FILE *file;
             uint8 mysector[512], tags[24], line[1024];
             unsigned long sector_number=0, last_sector=0x00fffffd, offset=-4;

             file=fopen(cargs[0],"r");

             printf("Opening profile dump: %s for reading into disk image.\n",cargs[0]);
             if (errno) perror("Could not open file.\n");

              while (!feof(file))
              {

                fgets((char *)line,1024,file);

                if ( strncmp((char *)line,"TAGS: ",5)==0)
                {int i=6,j=0;

                 while (line[i] && j<20)
                 { tags[j]=hex2byte(line[i],line[i+1]); //printf("Tag[%d]=[%c%c]==%02x\n",j,line[i],line[i+1],tags[j]);
                   i+=2; j++;

                   while (line[i]==',') i++;

                 }
                }


                if ( strncmp((char *)line,"Sector Number: ",14)==0)
                {
                 sector_number=hex2long(&line[15]);

                 if (sector_number & 0x00800000) sector_number |=0xff000000;  // ProFile sector #'s are 24 bit, so sign extend.


                 //printf("New sector:%08x previous sector:%08x\n",sector_number,last_sector);

                 if (last_sector+1!=sector_number) printf("DANGER! unexpected sector #%d wanted %d\n",sector_number,last_sector);

                 offset=-4; last_sector=sector_number;
                }

                if (line[8]==':' && line[17]==',' && line[26]==',' && line[35]==',' && line[44]==',')
                {
                 uint32 newoffset, c1,c2,c3,c4;

                  newoffset=hex2long(line);

                  c1=hex2long(&line[ 9]);
                  c2=hex2long(&line[18]);
                  c3=hex2long(&line[27]);
                  c4=hex2long(&line[36]);

                 if (newoffset<0x7d)
                    {
                     int i;

                       mysector[(newoffset +  0)*4 +  0]=(uint8)( (c1>>24) & 0x000000ff);
                       mysector[(newoffset +  0)*4 +  1]=(uint8)( (c1>>16) & 0x000000ff);
                       mysector[(newoffset +  0)*4 +  2]=(uint8)( (c1>> 8) & 0x000000ff);
                       mysector[(newoffset +  0)*4 +  3]=(uint8)( (c1    ) & 0x000000ff);

                       mysector[(newoffset +  1)*4 +  0]=(uint8)( (c2>>24) & 0x000000ff);
                       mysector[(newoffset +  1)*4 +  1]=(uint8)( (c2>>16) & 0x000000ff);
                       mysector[(newoffset +  1)*4 +  2]=(uint8)( (c2>> 8) & 0x000000ff);
                       mysector[(newoffset +  1)*4 +  3]=(uint8)( (c2    ) & 0x000000ff);

                       mysector[(newoffset +  2)*4 +  0]=(uint8)( (c3>>24) & 0x000000ff);
                       mysector[(newoffset +  2)*4 +  1]=(uint8)( (c3>>16) & 0x000000ff);
                       mysector[(newoffset +  2)*4 +  2]=(uint8)( (c3>> 8) & 0x000000ff);
                       mysector[(newoffset +  2)*4 +  3]=(uint8)( (c3    ) & 0x000000ff);

                       mysector[(newoffset +  3)*4 +  0]=(uint8)( (c4>>24) & 0x000000ff);
                       mysector[(newoffset +  3)*4 +  1]=(uint8)( (c4>>16) & 0x000000ff);
                       mysector[(newoffset +  3)*4 +  2]=(uint8)( (c4>> 8) & 0x000000ff);
                       mysector[(newoffset +  3)*4 +  3]=(uint8)( (c4    ) & 0x000000ff);

                       //printf("%08x:(%08x) data:",newoffset*4,newoffset);
                       //for (i=newoffset*4; i<(newoffset*4+16) ; i++)
                       //       printf("%02x,",mysector[i]);
                       //printf("|");
                       //for (i=newoffset*4; i<(newoffset*4+16) ; i++) printf("%c",niceascii(mysector[i]));
                       //puts("");

                       if (newoffset==0x7c && (sector_number<F->numblocks || sector_number==0xffffffff))
                            {
                              i=dc42_write_sector_tags(F, sector_number+1, tags);
                              if (i) printf("Wrote sector #%ld tags as sector #%ld tags status:%d %s",sector_number,sector_number+1,i,F->errormsg);

                              i=dc42_write_sector_data(F, sector_number+1, mysector);
                              if (i) printf("Wrote sector #%ld as sector #%ld status:%d %s",sector_number,sector_number+1,i,F->errormsg);
                            }
                    }

                 if (offset+4!=newoffset) printf("Sector#:%d Unexpected offset:%08x (wanted:%08x) data might be corrupted\n",newoffset,offset+4,sector_number);
                 offset=newoffset;
                }

                }
               fclose(file);
             }
             break;


       case DIFF2IMG_CMD :
            {
              DC42ImageType F2;
              int sec, count, i, tagsize, secsize, differs=0;
              uint8 *img1, *img2;
               { // open/convert the second image.
                if (dart_is_valid_image(cargs[0]))
                {
                 char dc42filename[FILENAME_MAX];

                 fprintf(stderr,"Checking to see if this is a DART image\n");
                 strncpy(dc42filename,cargs[0],FILENAME_MAX);
                 if   (strlen(dc42filename)<FILENAME_MAX-6)   strcat(dc42filename,".dc42");
                 else                                         strcpy( (char *)(dc42filename+FILENAME_MAX-6),".dc42");

                 printf("Converting DART image %s to DiskCopy42 Image:%s\n",cargs[0],dc42filename);
                 i=dart_to_dc42(cargs[0],dc42filename);
                 if (!i)   i=dc42_open(&F2, dc42filename, "w");
                           if (i) {perror("Couldn't open the disk image."); break;}
                }
                else  if (dc42_is_valid_image(cargs[0]))
                {
                  fprintf(stderr,"This is a recognized DC42 image, opening it.");
                  i=dc42_open(&F2, cargs[0], "w");
                  if (i) {perror("Couldn't open the disk image."); break;}
                }
                else
                {
                   fprintf(stderr,"Cannot recognize this disk image as a valid DC42 or DART image\n");
                   exit(1);
                }
                if (i) break;
               }

               if (F2.numblocks!=F->numblocks)
                    {printf("Warning! Disk images are different sizes!\n %5d blocks in %s,\n%5d blocks in %s\n\n",
                            F->numblocks,F->fname,  F2.numblocks, F2.fname);  differs=1;}

               if (F2.datasize!=F->datasize)
                    {printf("Warning! Disk images have different sized sectors!\n %5d bytes/sec in %s,\n%5d bytes/tag in %s\n\n",
                            F->datasize,F->fname,  F2.datasize, F2.fname);    differs=1;}

               if (F2.tagsize!=F->tagsize)
                    {printf("Warning! Disk images have different sized tags!\n %5d bytes/tag in %s,\n%5d bytes/tag in %s\n\n",
                            F->tagsize,F->fname,  F2.tagsize, F2.fname);      differs=0;}

               printf("Legend:<%s\n>>%s\n",F->fname,F2.fname);


              count=MIN(F2.numblocks,F->numblocks);
              tagsize=MIN(F2.tagsize,F->tagsize);
              secsize=MIN(F2.datasize,F->datasize);

              if (F2.tagsize==F->tagsize)
               for (sec=0; sec<count; sec++)
               {
                 if (tagsize)
                 {
                  img1=dc42_read_sector_tags(F,sec);
                  img2=dc42_read_sector_tags(&F2,sec);
                  if (img1!=NULL && img2!=NULL)
                     for (i=0; i<tagsize; i++)
                       if (img1[i]!=img2[i]) {printf("sec:%4d tag offset #%2d <%02x >%02x\n",sec,i,img1[i],img2[i]); differs=1;}
                 }

                 if (secsize)
                 {
                  img1=dc42_read_sector_data(F,sec);
                  img2=dc42_read_sector_data(&F2,sec);
                  if (img1!=NULL && img2!=NULL)
                     for (i=0; i<secsize; i++)
                       if (img1[i]!=img2[i]) {printf("sec:%4d data offset #%03x <%02x >%02x and more...\n",sec,i,img1[i],img2[i]);
                                              i=secsize; differs=1;}}
                 }

               dc42_close_image(&F2);
               if (!differs) puts("Sectors and tags between these images are identical.");
               }
               break;
       case VOLNAME_CMD :
              {
                int i;
                  printf("Current volume name is:\"%s\"\n",dc42_get_volname(F));
                  if (cargs[0]!=NULL)
                  {
                     //012345678
                     //volname_
                     i=dc42_set_volname(F,&cmd_line[8]);
                     if (i) printf("Could not set new volume name to \"%s\".\n",&cmd_line[8]);
                     else  printf("New volume name is:\"%s\"\n",dc42_get_volname(F));
                  }

              }
              break;
       case  QUIT_CMD:
            {
             fprintf(stderr,"Closing Disk Image\n");
             dc42_close_image(F);
             fprintf(stderr,"bye.\n");
             exit (0);
            }



       case  HELP_CMD:

           version_banner();
           //              1         2         3         4         5         6         7         8
           //    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
           puts("NOTE: This program relies on tag data to perform commands marked by a + sign.");
           puts("It also requires that the disk images be imaged by Apple Disk Copy 4.2.  It");
           puts("can't use DART images.  If you know DART's format details, please contact me.");
           puts("Versions of ADC newer than 4.2 do not extract tags!  Although they claim to be");
           puts("able to convert DART to DC4.2, they strip off tags, rendering them useless.");
           puts("");
           puts("    It only understands Lisa disk images with tags made by Disk Copy 4.2");
           puts("");
           puts("!command        - shell out and run command");
           puts("help            - displays this help screen.");
           puts("version         - displays version and copyright.");
           puts("{sector#}       - jump and display a sector #. (press ENTER for next one.)");
           puts("+|-{#}          - skip forward/backward by # of sectors.");

           puts("editsector {offset} {hexdata...}");
           puts("                - edit the current sector by writing the hex data at offset");
           puts("                  the change is immediately written to the file.");
           puts("             i.e. 'editsector 0x0F CA FE 01' writes bytes CA FE 01 offset 15");

           puts("edittag    offset {hexdata...}");
           puts("                - edit the current sector's tags by writing the hex data at the");
           puts("                  the offset.  the change is immediately written to the file.");
           puts("             i.e. 'editsector 4 00 00' writes bytes 0000 at offset 4 ");
           puts("                  (the example marks the FILEID tag as an unallocated block)");

           puts("difftoimg filename");
           puts("                - compare the currently opened disk image to another.");

           puts("loadsec offset filename");
           puts("                - load 512 bytes from the file {filename} at {offset} into");
           puts("                  the current sector number, and write that sector to the");
           puts("                  currently opened disk image. {offset} may be hex/dec/octal");
           puts("                  i.e. 'loadsec 0x1000 myfile.bin' opens the myfile.bin,");
           puts("                  reads the 512 bytes at offset 4096-4607 into the current");
           puts("                  sector, and writes the data to the currently opened image.");
           puts("loadbin offset filename");
           puts("                - load an entire file {filename} starting at {offset} into");
           puts("                  the current sector number and subsequent sectors of the");
           puts("                  currently opened disk image. {offset} may be hex/dec/octal");
           puts("                  i.e. 'loadbin 0x1000 myfile.bin' opens the myfile.bin,");
           puts("                  reads the first 512 bytes from offset 4096-4607 into the");
           puts("                  current sector, the next into the next sectorm until the");
           puts("                  end of file or last sector in the image is reached.");
           puts("loadprofile filename");
           puts("                - parse and load a ProFile hex dump received from nanoBug");
           puts("                  into the current disk image.  Sector FFFFFF from the dump");
           puts("                  becomes sector 0, sector 0 in the dump becomes sec 1, etc.");
           puts("                  Format of the hex dump is:\n");
           puts("                  Sector Number: 00FFFFFF");
           puts("                  TAGS: 00000000,00000000,00000000,00000000,00000000");
           puts("                  00000000:00021420,03000001,00000281,0002C1FF,");
           puts("                  00000004:FFFFFFFF,FFFFFFFF,FFFFFFFF,FFFFFFFF,");
           puts("                  00000008:FFFFFFFF,FFFFFFFF,FFFFFFFF,FFFFFFFF,");
           puts("                  ...");
           puts("                  0000007C:FFFFFFFF,FFFFDB6D,DB6DDB6D,DB6DDB6D,");
           puts("                  data is index:data,data,data,data where index");
           puts("                  is the offset/4.");
           puts("");





           //              1         2         3         4         5         6         7         8
           //    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789

           puts("display         - display a sector #");
         //puts("cluster         - display the 1st sector in a cluster.");
         //puts("setclustersize  - set the cluster size.");
           puts("dump            - dump all sectors and tags in hex display");
           puts("n/p             + next/previous display in sorted chain.");
           puts("tagdump         + dump tags");
           puts("sorttagdump     + sort tags by file ID and sector #, then dump them");
           puts("sortdump        + same as above, but also output actual sectors");
           puts("bitmap          + show block allocation bitmap");
           puts("extract         + extracts all files on disk based on tag data.");
           puts("                  files are written to the current directory and named based");
           puts("                  on the file id and Disk Copy image name.");
           puts("dir             + list tag file ID's and extracted file names from catalog.");
           puts("volname {newvolname}");
           puts("                - if a new volume name is provided, it's written to the image");
           puts("                  otherwise, the current volume name is displayed.  The default");
           puts("                  volname is \"-not a Macintosh disk-\" for non MFS/HFS img's");
           puts("quit            - exit program.");
           puts("");
           break;

           //              1         2         3         4         5         6         7         8
           //    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
   }
 }
}



int main (int argc, char *argv[])
{
int i=0,argoffset=0;

    version_banner();
    if (argc<2)
    {
      puts("Usage: lisafsh file.dc42 to open an existing DiskCopy 4.2 image");
      puts(" or    lisafsh --new file.dc42 size to create a new image and open it.");
      puts("");
      puts("valid sizes are: 400k 800k 860k 720k 1440k 2880k 5m 10m 20m");
      puts("a default of 12 byte tags will be added to 400k/800k/860K images.");
      puts("if the image size is 720k/1440k/2880k - no tags will be added.");
      puts("if the image size is over 5mb, it will be created with 24 byte tags.");
      puts("");
      puts("i.e.   lisafsh --new file.dc42 400k - create a single sided new image");
      puts("i.e.   lisafsh --new profile5.dc42 5m - create a 5mb image");
      exit(0);
    }

    //F.fhandle=fopen(argv[1],"rb");
    //F.filename=argv[1];

    if (strncmp(argv[1],"--new",8)==0)
    {
      int i,size=-1, tagsize=-1;
      argoffset=1;

      // GCR Lisa/Mac disk sizes
      if (strncmp(argv[3], "400k",7)==0) {size=     800*512; tagsize= 800*12;     }
      if (strncmp(argv[3], "800k",7)==0) {size=    1600*512; tagsize=1600*12;     }
      if (strncmp(argv[3], "860k",7)==0) {size=    1720*512; tagsize=1720*12;     }

      // MFM PC disk sizes
      if (strncmp(argv[3], "720k",7)==0) {size=    1440*512; tagsize=0;           }
      if (strncmp(argv[3],"1440k",7)==0) {size=    2880*512; tagsize=0;           }
      if (strncmp(argv[3],"2880k",7)==0) {size=    5760*512; tagsize=0;           }

      // ProFile/Widget.  9728+1 for spares
      if (strncmp(argv[3],"5m"   ,7)==0) {size=    9728*512; tagsize=9729*24; }
      if (strncmp(argv[3],"10m"  ,7)==0) {size=  2*9728*512; tagsize=size/512*24; }
      if (strncmp(argv[3],"20m"  ,7)==0) {size=20*1024*1024; tagsize=size/512*24; }
      if (strncmp(argv[3],"30m"  ,7)==0) {size=30*1024*1024; tagsize=size/512*24; }

      if (size==-1) {printf("Invalid size parameter. Expected 400k 800k 860k 720k 1440k 2880k 5m 10m 20m\n"); exit(3);}

      i=dc42_create(argv[1+argoffset],"-not a Macintosh disk-",size,tagsize);
      if (!i) {printf("New Disk Image:%s created successfully. (%d data bytes, %d tag bytes)\n",
               argv[1+argoffset],
               size,
               tagsize);
               }
      else
               {
                 printf("Could not create the disk image. error %d returned\n",i); perror("");
                 exit(3);
               }
    }

    i=dc42_auto_open(&F,argv[1+argoffset],"wb");
    if (i)
    {
       fflush(stderr); fflush(stdout);
       fprintf(stderr,"Cannot recognize this disk image as a valid DC42 or DART (Fast-Compressed) image:%d\n%s",i,F.errormsg);
       fflush(stderr); fflush(stdout);
       exit(1);
    }

    printf("File size:     %d\n",  F.size);
    printf("Filename:      %s\n",  F.fname);
    printf("Sectors:       %d\n",  F.numblocks);

    printf("Tag size each: %d\n",  F.tagsize);
    printf("Sec size each: %d\n",  F.sectorsize);
    printf("Sector bytes:  %d\n",  F.datasizetotal);
    printf("Tag bytes:     %d\n",  F.tagsizetotal);

    printf("Image Type:    %d (0=twig, 1=sony400k, 2=sony800k)\n",  F.ftype);
    printf("Tagstart @     %d\n", F.tagstart);
    printf("Sectorstart @  %d\n", F.sectoroffset);
    printf("Maxtrack:      %d\n", F.maxtrk);
    printf("Maxsec/track:  %d\n", F.maxsec);
    printf("Max heads:     %d\n", F.maxside);
    //printf("Image mmapped  @%p\n", F.RAM);
    printf("Calc DataChks: %08x\n",dc42_calc_data_checksum(&F));
    printf("Img DataChks:  %02x%02x%02x%02x\n",
                                              F.RAM[72],
                                              F.RAM[73],
                                              F.RAM[74],
                                              F.RAM[75]);



    printf("Calc TagChks:  %08x\n",dc42_calc_tag_checksum(&F));
    printf("Calc TagChk0:  %08x\n",dc42_calc_tag0_checksum(&F));

    printf("Img TagChks:   %02x%02x%02x%02x\n",
                                               F.RAM[76],
                                               F.RAM[77],
                                               F.RAM[78],
                                               F.RAM[79]);



    // clear the volume name, sorting the tags will set it.
    memset(volumename,0,31);
    memset(directory,0,65535);

    if (dc42_has_tags(&F))
       if (!tagsaresorted) {
                             fprintf(stderr,"Sorting tags\n");
                             sorttags(&F);   tagsaresorted=1;
                             havetags=1;
                           }



    if (i) {fprintf(stderr,"Had trouble reading the disk image, code:%d",i);}

    //printf("Pointer to RAM:%p",F.RAM);
    cli(&F);
    dc42_close_image(&F);
return 0;
}
