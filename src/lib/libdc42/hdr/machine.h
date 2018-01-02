/**************************************************************************************\
*                                                                                      *
*                machine.h -  detected integer types for this host                     *
*                              http://lisaem.sunder.net                                *
*                                                                                      *
*                   Copyright (C) 1998, 2010 Ray A. Arachelian                         *
*                                All Rights Reserved                                   *
*                                                                                      *
\**************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#ifndef IN_MACHINE_H
#define IN_MACHINE_H 1


typedef          char         int8;
typedef unsigned char        uint8;
typedef          short       int16;
typedef unsigned short      uint16;
typedef          int         int32;
typedef unsigned int        uint32;
typedef          long        int64;
typedef unsigned long       uint64;

typedef          int8         sint8;
typedef          int16       sint16;
typedef          int32       sint32;
typedef          int64       sint64;

#endif
