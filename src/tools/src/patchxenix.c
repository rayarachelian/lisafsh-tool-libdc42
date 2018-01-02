/**************************************************************************************\
*                                                                                      *
*                    Xenix Boot Floppy Patcher for X/ProFile                           *
*                                                                                      *
\**************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <libdc42.h>

extern int patchXenix(DC42ImageType *F, int do_patch);


int main(int argc, char *argv[])
{
  int i,j,err,stat;
  DC42ImageType  F;
  char *Image=NULL;

  int do_patch=15;
  for (i=1; i<argc; i++)
   {
     if (strcmp(argv[i],"-a")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=1;  do_patch &=~8;
        }
     else
     if (strcmp(argv[i],"-b")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=2;  do_patch &=~16;
        }
     else
     if (strcmp(argv[i],"-c")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=4;  do_patch &=~32;
        }
     else
     if (strcmp(argv[i],"+a")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=8; do_patch &=~1;
        }
     else
     if (strcmp(argv[i],"+b")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=16;  do_patch &=~2;
        }
     else
     if (strcmp(argv[i],"+c")==0)
        {if (do_patch==15) do_patch=0;
         do_patch |=32;  do_patch &=~4;
        }
     else
     if (strcmp(argv[i],"-n")==0)
        {
         do_patch=-1;
        }
     else
     if (strcmp(argv[i],"-h")==0 || strcmp(argv[i],"--help")==0 )
        {
            printf("Patch Xenix Utility. see: http://sigmasevensystems.com/xpf_xenix.html\n\n"
                //  01234567890123456789012345678901234567890123456789012345678901234567890123456789
                //  0         1         2         3         4         5         6         7         8
                   "Usage: patchxenix {-a} {-b} {-c} {-n} Xenix_Boot_Disk.dc42\n\n"

                   "       -a always send 6 command bytes.                +a revert patch a\n"
                   "       -b don't assert CMD before BSY IRQ enable      +b revert patch b\n"
                   "       -c assert CMD after BSY IRQ + Fast             +c revert patch c\n"
                   "       -n Don't patch, just report status.      \n\n"
                   "This utility is used to patch the Microsoft/SCO Xenix Boot Disk\n"
                   "in order to make it more compatible with Sigma Seven Systems' X/ProFile.\n"
                   "and other software such as LisaEm.\n"
            );

            exit(0);
        }
     else
        {
            if (!Image) Image=argv[i];
            else {fprintf(stderr,"Can only specify one filename.  Surround it by quotes if it contains spaces.\n"); exit(2);}
        }
   }
  if (do_patch<0) do_patch=0;

  err=dc42_auto_open(&F,Image,"wb");
  if (err)
     {
          fprintf(stderr,"could not open: '%s' because:%s",Image,F.errormsg);
          exit(1);
     }

   if (do_patch) patchXenix(&F, do_patch);

   stat=patchXenix(&F,0);
   printf("%s is %spatched to (a) always send 6 command bytes\n",Image,                    (stat & 1  ? "un":""));
   printf("%s is %spatched to (b) not assert CMD before the BSY IRQ Enable\n",Image,       (stat & 2  ? "un":""));
   printf("%s is %spatched to (c) assert CMD after the BSY IRQ Enable\n",Image,            (stat & 4  ? "un":""));

  dc42_close_image(&F);
  return 0;
}
