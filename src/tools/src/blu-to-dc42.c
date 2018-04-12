/**************************************************************************************\
*                                                                                      *
*                        blu2dc42 profile hd image converter                           *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                                                                                      *
*                        Copyright (C) 2011 Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
\**************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "../hdr/libdc42.h"


// ---------------------------------------------

long interleave5(long sector)
{
  static const int offset[]={0,5,10,15,4,9,14,3,8,13,2,7,12,1,6,11,16,21,26,31,20,25,30,19,24,29,18,23,28,17,22,27};
  return offset[sector&31] + sector-(sector&31);
}



long deinterleave5(long sector)
{
  static const int offset[]={0,13,10,7,4,1,14,11,8,5,2,15,12,9,6,3,16,29,26,23,20,17,30,27,24,21,18,31,28,25,22,19};
  return offset[sector&31] + sector-(sector&31);
}

int main(int argc, char *argv[])
{
    long numblocks,i,b;
    FILE *blu;
    DC42ImageType profile;

    char dc42filename[8192]; 
    uint8 block[533];

    if (argc<2)
    {
     puts("\n"
//345678901234567890123456789012345678901234567890123456789012345678901234567890
//       1         2         3         4         5         6         7
"                          blu2dc42 V0.0.1 Profile Drive Converter\n"
"                              http://lisaem.sunder.net\n"
"\n"
"                           Copyright (C) 2018 Ray A. Arachelian  \n"
"                                    All Rights Reserved.          \n"
"\n"
"  Usage: blu2dc42 profile.blu\n"
"\n"
"  This program takes a BLU ProFile/Widget/Priam disk image and\n"
"  converts it to dc42 format usable by LisaEm\n"
"\n"
"\n");
    exit(1);
    }

    blu=fopen(argv[1],"rb");
    if (!blu) {perror("Could not open the reverse blu image."); exit(2);}

    snprintf(dc42filename,8192,"%s.dc42",argv[1]);
    fseek(blu, 0L, SEEK_END);
    numblocks=ftell(blu);  // in bytes until we read blocksize
    fseek(blu, 0L, SEEK_SET);
    
    /* Read BLU header block */
    i=fread(block,512+20,1,blu);
    if (i!=1) {fprintf(stderr,"\n\nWARNING: Error reading BLU header block, fread size did not return 1 block, got %d blocks!\n",i); }

    char *blusign=&block[512];
    blusign[15]=0; // force terminate string
    int blublocks=( (block[0x12]<<16) | (block[0x13]<<8) | (block[0x14]    ) );
    int blocksize=(                     (block[0x15]<<8) | (block[0x16]    ) );

    numblocks=numblocks/blocksize; // correct to blocks

    if (strncmp(blusign,"Lisa HD Img BLU",15)!=0)
    {
      fprintf(stderr,"This doesn't appear to be a BLU image\n");
      fclose(blu);
      exit(2);
    }

    if (strlen(block)<16)
    {
      fprintf(stderr,"Drive image is from: %s of type %02x%02x%02x\n",block,block[0x0D],block[0x0E],block[0x0F]);
      fprintf(stderr,"Number of Blocks:%d size of each block:%d\n",blublocks,blocksize);
    }

    fseek(blu, blocksize, SEEK_SET);  // not sure about this -2

    numblocks--; // skip header 

    if (abs(numblocks - blublocks)>2) // might have extras at the end due to xmodem buffering
    {
      fprintf(stderr,"Warning: number of blocks in BLU header (%d) doesn't match size of file (%d)\nUsing blublocks value.\n",blublocks,numblocks);
      numblocks=blublocks;
    }

    i=dc42_create(dc42filename,"-lisaem.sunder.net hd-", numblocks*512,numblocks*20);
    if (i) {fprintf(stderr,"Could not create DC42 ProFile:%s\n",dc42filename); perror(" "); exit(1);}

    i=dc42_open(&profile,dc42filename,"wm");
    if (i) {fprintf(stderr,"Could not open created DC42 ProFile:%s because %s\n",dc42filename,profile.errormsg); exit(1); }

    for (b=0; b<numblocks; b++)
    {
      //long b5=deinterleave5(b);
      long b5=b;
      printf ("reading BLU block %04d, writing to profile block %04d \r",b,b5);
      i=fread(block,blocksize,1,blu);
      if (i!=1) 
      {
         dc42_close_image(&profile);
         puts ("                                                               \rError.\n");
         fclose(blu);
         fprintf(stderr,"\nError reading block # %d, fread size did not return 1 block, got %d blocks!\n",b,i); 
         exit(1);
      }

      i=dc42_write_sector_data(&profile,b5, &block[0]);
      if (i) {fprintf(stderr,"\n\nError writing block data %d to dc42 ProFile:%s because %s\n",b,dc42filename,profile.errormsg); dc42_close_image(&profile); exit(1); }

      block[510]=0; block[511]=0; // missing volid tags
      i=dc42_write_sector_tags(&profile,b5, &block[510]);  // yes this is off by 2 bytes  
      if (i) {fprintf(stderr,"\n\nError writing block tags %d to dc42 ProFile:%s because %s\n",b,dc42filename,profile.errormsg); dc42_close_image(&profile); exit(1); }

     }
     dc42_close_image(&profile);
     puts ("                                                               \rDone.");
     fclose(blu);
     return 0;
}
