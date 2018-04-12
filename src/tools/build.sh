#!/bin/bash 

#### Edit these options for your system

WITHDEBUG=""             # -g for debugging, -p for profiling. -pg for both

#STATIC=--static
WITHOPTIMIZE="-O2 -ffast-math -fomit-frame-pointer"
WITHUNICODE="--unicode=no"

#if compiling for win32, edit WXDEV path to specify the
#location of wxDev-CPP 6.10 (as a cygwin, not windows path.)
#i.e. WXDEV=/cygdrive/c/wxDEV-Cpp
#if left empty, code below will try to locate it, so only set this
#if you've installed it in a different path than the default.

#WXDEV=""


########################################################################
export VERSION="0.9.7"

for i in get-uintX-types.c libdc42-gpl-license.txt src/blu-to-dc42.c  src/dc42-to-raw.c  src/dumper.c  src/lisadiskinfo.c  src/lisafsh-tool.c  src/patchxenix.c  src/raw-to-dc42.c  src/rraw-to-dc42.c  src/xenpatch.c
do
 if [ ! -f ./$i ]
 then
   echo Could not find $i in `pwd`
   echo
   echo "Please run this script from the top level directory. i.e."
   echo
   echo "tar xjpvf tools-${VERSION}.tar.bz2"
   echo "cd tools-${VERSION}"
   echo "./build.sh $@"
   exit 1
 fi
done


# Include and execute unified build library code
if [ -x ./src.build ]
then
 source ./src.build
else
 echo "Cannot find src.build include file." 1>&2
 echo "This is required as it contains the shared code for the unified build system scripts." 1>&2
 exit 1
fi




# turn this on by default.

for i in $@
do
 [ "$i" == "--no-banner" ] && NOBANNER=1;
done

if [ -z "$NOBANNER" ]
then

echo
echo '----------------------------------------------------------------'
echo "           dc42 tools ${VERSION}  -   Unified Build Script"
echo
echo '                http://lisaem.sunder.net'
echo '    Copyright (C) 2008 Ray Arachelian, All Rights Reserved'
echo 'Released under the terms of the GNU General Public License 2.0'
echo '----------------------------------------------------------------'

fi


# Parse command line options if any, overriding defaults.

for i in $@
do

 case "$i" in
  clean)
            echo "* Removing fsh tools objs and bins"
            rm -f .last-opts last-opts
            cd ./bin   && /bin/rm -f *
            cd ../obj  && /bin/rm -f *.a *.o get-uintX-types*
            cd ..

            #if we said clean install or clean build, then do not quit
            Z="`echo $@ | grep -i install``echo $@ | grep -i build`"
            [ -z "$Z" ] && exit 0

  ;;
 build*)    echo ;;    #default - nothing to do here, this is the default.
 install)
            [ -z "$CYGWIN" ] && [ "`whoami`" != "root" ] && echo "Need to be root to install. try sudo ./build.sh $@" && exit 1
            INSTALL=1;
            ;;

 uninstall)
           if [ -n "$DARWIN" ];
           then
             echo Uninstall commands not yet implemented.
             exit 1
           fi

           if [ -n "$CYGWIN" ];
           then
              [ -n "$PREFIX" ]    && echo Deleting $PREFIX    && rm -rf $PREFIX
              [ -n "$PREFIXLIB" ] && echo Deleting $PREFIXLIB && rm -rf $PREFIXLIB
              exit 0
           fi

           #Linux, etc.

           #PREFIX="/usr/local/bin"
           #PREFIXLIB="/usr/local/share/"

           echo Uninstalling from $PREFIX and $PREFIXLIB
           rm -rf $PREFIXLIB/lisaem/
           rm -f  $PREFIX/lisaem
           rm -f  $PREFIX/lisafsh-tool
           rm -f  $PREFIX/lisadiskinfo
           rm -f  $PREFIX/patchxenix

           exit 0

    ;;
#DEBUG DEBUGMEMCALLS IPC_COMMENTS

 --32)                         SIXTYFOURBITS=""                          ;;
 --64)                         SIXTYFOURBITS="--64"                      ;;

 --without-debug)              WITHDEBUG=""
                               WARNINGS=""                               ;;

 --with-debug)                 WITHDEBUG="$WITHDEBUG -g"
                               WARNINGS="-Wall"                          ;;


 --with-profile)               WITHDEBUG="$WITHDEBUG -p"                 ;;

 --with-tracelog)              WITHTRACE="-DDEBUG -DTRACE"
                               WARNINGS="-Wall"                          ;;
 --no-banner)                  NOBANNER="1";                             ;;
 *)                            UNKNOWNOPT="$UNKNOWNOPT $i"               ;;
 esac

done



if [ -n "$UNKNOWNOPT" ]
then
 echo
 echo "Unknown options $UNKNOWNOPT"
 cat <<ENDHELP

Commands:
 clean                    Removes all compiled objects, libs, executables
 build                    Compiles lisaem (default)
 clean build              Remove existing objects, compile everything cleanly
 install                  Not yet implemented on all platforms
 uninstall                Not yet implemented on all platforms

Debugging Options:
--without-debug           Disables debug and profiling
--with-debug              Enables symbol compilation

Other Options:
--no-banner               Suppress version/copyright banner

Environment Variables you can pass:

CC                        Path to C Compiler
CPP                       Path to C++ Compiler
WXDEV                     Cygwin Path to wxDev-CPP 6.10 (win32 only)
PREFIX                    Installation directory

i.e. CC=/usr/local/bin/gcc ./build.sh ...

ENDHELP
exit 1

fi

[ -n "${WITHDEBUG}${WITHTRACE}" ] && if [ -n "$INSTALL" ];
then
   echo "Warning, will not install since debug/profile/trace options are enabled"
   echo "as Install command is only for production builds."
   INSTALL=""
fi

if [ -n "${WITHDEBUG}${WITHTRACE}" ]
then
  WITHOPTIMIZE="-O2 -ffast-math"  #disable optimizations that break gdb operations
fi

#if we're not on Cygwin, then setup the defaults, unless
#they were defined already from the parent shell.
if [ -z "$CYGWIN" ]
then
 [ -z "$CC" ] && CC=gcc
 [ -z "$CXX" ] && CXX=g++
 [ -z "$GPROF" ] && GPROF=gprof
fi

###########################################################################

# Has the configuration changed since last time? if so we may need to do a clean build.
[ -f .last-opts ] && source .last-opts

needclean=0

MACHINE="`uname -mrsv`"
[ "$MACHINE"   != "$LASTMACHINE" ] && needclean=1
#debug and tracelog changes affect the whole project, so need to clean it all
[ "$WITHTRACE" != "$LASTTRACE" ] && needclean=1
[ "$WITHDEBUG" != "$LASTDEBUG" ] && needclean=1
# display mode changes affect only the main executable.

if [ "$needclean" -gt 0 ]
then
   rm -f .last-opts last-opts
   cd bin         && /bin/rm -f *
   cd ../obj      && /bin/rm -f *.a *.o
   cd ..

fi

echo "LASTDEBUG=\"$WITHDEBUG\""  >>.last-opts
echo "LASTMACHINE=\"$MACHINE\""  >>.last-opts

###########################################################################

if [ -n "$DARWIN" ]
then
   if [ -n "$SIXTYFOURBITS" ]
   then
     #echo 64 bits on
     CFLAGS="$CFLAGS     -arch x86_64 -m64"
     CPPFLAGS="$CFLAGS   -arch x86_64 -m64"
     CXXFLAGS="$CXXFLAGS -arch x86_64 -m64"
   else
     #echo 32 bits on
     CFLAGS="$CFLAGS     -arch i386 -m32"
     CPPFLAGS="$CFLAGS   -arch i386 -m32"
     CXXFLAGS="$CXXFLAGS -arch i386 -m32"
   fi
fi

     # CODEKARMA FIXME FIXME silence warnings for demos
     CFLAGS="$CFLAGS                -Wno-empty-body  -Wno-duplicate-decl-specifier  -Wno-constant-logical-operand  -Wno-incompatible-pointer-types  -Wno-implicit-function-declaration   -Wno-enum-conversion   -Wno-unsequenced   -Wno-parentheses  -Wno-tautological-constant-out-of-range-compare  -Wno-format -Wno-implicit-function-declaration  -Wno-unused-parameter  -Wno-unused -Wno-bitfield-constant-conversion"


echo Building lisa disk utilities...
echo
WHICHLIBDC42="`ls ../lib/libdc42/lib/libdc42.*.a 2>/dev/null`"
if [ -z "$WHICHLIBDC42" ]
then
   [ -f "/usr/local/lib/libdc42.*.a" ] && WHICHLIBDC42="/usr/local/lib/libdc42.*.a" && DC42INCLUDE="/usr/local/include/libdc42.h"
   [ -f "/usr/lib/libdc42.*.a" ]       && WHICHLIBDC42="/usr/lib/libdc42.*.a" && DC42INCLUDE="/usr/include/libdc42.h"
else
   DC42INCLUDE="../../lib/libdc42/hdr"
fi

if [ -n "$WHICHLIBDC42" ]
then
   #echo "  Using $WHICHLIBDC42"
   WHICHLIBDC42="../$WHICHLIBDC42"
else
   cd ../lib/libdc42/ && ./build.sh --no-banner
   WHICHLIBDC42="`ls ../lib/libdc42/lib/libdc42.*.a 2>/dev/null`"
   if [ ! -f "$WHICHLIBDC42" ]; then exit 1; fi
   DC42INCLUDE="../../lib/libdc42/hdr"
   WHICHLIBDC42="../$WHICHLIBDC42"
fi


[ ! -f ./hdr/machine.h ] && needclean=1

if [ "$needclean" -gt 0 ]
then

 $CC -Wall get-uintX-types.c -o ./obj/get-uintX-types || exit 1
 cd ./hdr
 ../obj/get-uintX-types
 if [ "$?" -ne 0 ]; then
    echo something went wrong finding type information.
    exit 1
 fi
cd ..
fi

cd src


for i in blu-to-dc42 dc42-to-raw dumper lisadiskinfo lisafsh-tool xenpatch patchxenix raw-to-dc42 rraw-to-dc42 
do

  LIST="$LIST ../obj/$i.o"

  DEPS=0
  [ "$DEPS" -eq 0 ] && if NEEDED ${i}.c            ../obj/${i}.o;then DEPS=1; fi
  if [ "$DEPS" -gt 0 ]
  then
     echo "  Compiling ${i}.c..."
     #pwd
     #which $CC
     #echo
     $CC -W $WARNINGS -Wstrict-prototypes -I $DC42INCLUDE -I./hdr -Wno-format -Wno-unused  $WITHDEBUG $WITHTRACE $CFLAGS -c ${i}.c -o ../obj/${i}.o || exit 1
     if [ "$i" == "patchxenix" ]
     then
        $CC -W $WARNINGS -Wstrict-prototypes -I $DC42INCLUDE -I./hdr -Wno-format -Wno-unused  $WITHDEBUG $WITHTRACE $CFLAGS ../obj/${i}.o  ../obj/xenpatch.o "$WHICHLIBDC42" -o ../bin/$i || exit 1
     else
        if [ "$i" != "xenpatch" ]
        then
          $CC -W $WARNINGS -Wstrict-prototypes -I $DC42INCLUDE -I./hdr -Wno-format -Wno-unused  $WITHDEBUG $WITHTRACE $CFLAGS ../obj/${i}.o                    "$WHICHLIBDC42" -o ../bin/$i || exit 1
        fi
     fi
  fi
done

###########################################################################

if [ -n "$INSTALL" ]
    then
      cd ../bin/
      [ -n "$DARWIN" ] && PREFIX=/usr/local/bin  # these shouldn't go into /Applications
      echo Installing tools to $PREFIX/bin
      mkdir -pm755 $PREFIX/bin 2>/dev/null
      for i in lisadiskinfo  lisafsh-tool  patchxenix  raw-to-dc42 dc42-to-raw
      do
           cp ${i}* $PREFIX/bin/ || exit 1
      done
      cd ..
    fi
echo
[ -z "$NOBANNER" ] && echo Build Done.
exit 0
