/**************************************************************************************\
*                                                                                      *
*                       libGenerator - integer size detector                           *
*                             http://lisaem.sunder.net                                 *
*                                                                                      *
*                  Copyright (C) 1998, 2010 Ray A. Arachelian                          *
*                                All Rights Reserved                                   *
*                                                                                      *
\**************************************************************************************/

//    Because there's nothing quite like the thrill of reinventing the wheel again!   //

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

int main(int argc, char *argv[])
{
FILE *out;
int sz[5];
int i;
int h[9];

h[0]=0;
h[1]=0;
h[2]=0;
h[3]=0;
h[4]=0;
h[5]=0;
h[6]=0;
h[7]=0;
h[8]=0;

char *szs[5]={"char", "short", "int", "long", "long long"};

out=fopen("machine.h","wt");
if (!out) {perror("Could not create machine.h because "); return -1; }

sz[0]=sizeof(char);
sz[1]=sizeof(short);
sz[2]=sizeof(int);
sz[3]=sizeof(long);
sz[4]=sizeof(long long);


fprintf(out,
"/**************************************************************************************\\\n"
"*                                                                                      *\n"
"*                machine.h -  detected integer types for this host                     *\n"
"*                              http://lisaem.sunder.net                                *\n"
"*                                                                                      *\n"
"*                   Copyright (C) 1998, 2010 Ray A. Arachelian                         *\n"
"*                                All Rights Reserved                                   *\n"
"*                                                                                      *\n"
"\\**************************************************************************************/\n\n"
"#include <stdio.h>\n"
"#include <stdlib.h>\n"
"#include <sys/types.h>\n\n\n"
"#ifndef IN_MACHINE_H\n"
"#define IN_MACHINE_H 1\n\n\n"
);



for (i=0; i<5; i++)
{
   if (! h[sz[i]] )
   {
         h[sz[i]]=1;
         fprintf(out,"typedef          %-9s %s  int%d;\n",szs[i],(sz[i]<2 ? " ":""),sz[i]*8); if (errno) {perror("couldn't write to machine.h because "); fclose(out); return -1;}
         fprintf(out,"typedef unsigned %-9s %s uint%d;\n",szs[i],(sz[i]<2 ? " ":""),sz[i]*8); if (errno) {perror("couldn't write to machine.h because "); fclose(out); return -1;}
   }
}


fprintf(out,"\n"
"typedef          int8         sint8;\n"
"typedef          int16       sint16;\n"
"typedef          int32       sint32;\n"
"typedef          int64       sint64;\n"
"\n#endif\n");

fclose(out);
if (h[1] && h[2] && h[4] && h[8]) return 0;

return -1;
}
