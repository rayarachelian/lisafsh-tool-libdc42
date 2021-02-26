/**************************************************************************************\
*                   A part of the Apple Lisa 2 Emulator Project                        *
*                                                                                      *
*                    Copyright (C) 2018  Ray A. Arachelian                             *
*                            All Rights Reserved                                       *
*                                                                                      *
*                     Deserialize Lisa Tools and Disk Label                            *
*                                                                                      *
\**************************************************************************************/

#include <libdc42.h>

void deserialize(DC42ImageType *F)
{
         uint8 *mddftag=dc42_read_sector_tags(F,28);
         uint8 *mddfsec=dc42_read_sector_data(F,28);

         // Remove Lisa signature on previously used Serialized Master Disks //////////////////////////////////////////////
         if (mddftag[4]==0 && mddftag[5]==1 &&  // MDDF Signature
             mddfsec[0x0d]=='O' &&
             mddfsec[0x0e]=='f' &&
             mddfsec[0x0f]=='f' &&
             mddfsec[0x10]=='i' &&
             mddfsec[0x11]=='c' &&
             mddfsec[0x12]=='e' &&
             mddfsec[0x13]==' ' &&
             mddfsec[0x14]=='S' &&
             mddfsec[0x15]=='y' &&
             mddfsec[0x16]=='s' &&
             mddfsec[0x17]=='t' &&
             mddfsec[0x18]=='e' &&
             mddfsec[0x19]=='m'    )
         {
             uint32 disk_sn=(mddfsec[0xcc]<<24) | (mddfsec[0xcd]<<16) | (mddfsec[0xce]<<8) | (mddfsec[0xcf]);

             if (disk_sn)
             {
                 uint8 buf[512];
                 memcpy(buf,mddfsec,512);
                 buf[0xcc]=buf[0xcd]=buf[0xce]=buf[0xcf]=0;
                 dc42_write_sector_data(F,28,buf);
                 printf("Disk MDDF signed by Lisa SN: %08x (%d) zeroed out.",disk_sn,disk_sn);
             }
     
         }
	 
         uint8 *ftag;
         uint8 *fsec;
         
         for (int sec=32; (unsigned)sec<F->numblocks; sec++)
         {
               char name[64];
               ftag=dc42_read_sector_tags(F,sec);

               if (ftag[4]==0xff)         // tool entry tags have tag 4 as ff
               {
                  fsec=dc42_read_sector_data(F,sec);
                  int s=fsec[0];                   // size of string (pascal string)
                  // possible file name, very likely to be the right size.  
                  // Look for {T*}obj.  i.e. {T5}obj is LisaList, but could have {T9999}obj but very unlikely
                  // also check the friendly tool name size at offset 0x182.
    
                  // T{12345678}OBJ
		  // 123456789012345 
                  if (s>6 && s<32 && fsec[1]=='{' && fsec[2]=='T' && fsec[3]>='0' && fsec[3]<='9'  &&
                         fsec[s-3]=='}' && tolower(fsec[s-2])=='o' && tolower(fsec[s-1])=='b' && tolower(fsec[s])=='j' && fsec[0x182]<64 )
                  {
                    s=fsec[0x182];                // Size of tool name
                    memcpy(name,&fsec[0x183],s);  // copy it over.
                    name[s]=0;                    // string terminator.

                    uint32 toolsn=(fsec[0x42]<<24) | (fsec[0x43]<<16) | (fsec[0x44]<<8) | fsec[0x45];
                    if (  toolsn != 0 || (fsec[0x48] | fsec[0x49] ) )     
                       {
                        fprintf(stderr, 
                             "Deserialized/debozo-ized Office System tool %s serialized for Lisa #%02x%02x%02x%02x bozo bits:%02x%02x\n",
                             name,
                             fsec[0x42], fsec[0x43], fsec[0x44], fsec[0x45], fsec[0x48], fsec[0x49]);
                       
                        uint8 buf[512];
                        memcpy(buf,fsec,512);
                        buf[0x42]=buf[0x43]=buf[0x44]=buf[0x45]=buf[0x48]=buf[0x49]=0;
                        dc42_write_sector_data(F,sec,buf);
                      } 
		    else fprintf(stderr,"Tool %s is not serialized and bozo flags are not turned on\n",name);
                  }
               }
         }

}




int main(int argc, char *argv[])
{
  int i,j;

  DC42ImageType  F;
  char creatortype[10];

      puts("  ---------------------------------------------------------------------------");
      puts("    Lisa Tool Deserializer v0.02                    http://lisaem.sunder.net");
      puts("  ---------------------------------------------------------------------------");
      puts("          Copyright (C) 2018, Ray A. Arachelian, All Rights Reserved.");
      puts("              Released under the GNU Public License, Version 2.0");
      puts("    There is absolutely no warranty for this program. Use at your own risk.  ");
      puts("  ---------------------------------------------------------------------------\n");

  if (argc<=1)
  {
      puts("");
      puts("  This program is used to deserialize and de-bozoize Lisa Office System disk images");
      puts("  that contain serialized tools (LOS Applications)");
      puts("");
      puts("  Usage:   losdeserialize {filename1} {filename2} ... {filename N}");
      puts("  i.e.   ./losdeserialize SomeImage.dc42 SomeOtherImage.dc42");
      exit(0);
  }


  for (i=1; i<argc; i++)
  {
     int ret=0;
     printf("\nDeserializing: %-50s ",argv[i]);

     ret=dc42_auto_open(&F, argv[i], "wb");
     if (!ret) deserialize(&F);
     else      fprintf(stderr,"Could not open image %s because %s\n",argv[i],F.errormsg);
     dc42_close_image(&F);
  }

return 0;
}

