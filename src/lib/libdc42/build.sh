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
export VERSION="0.9.8"

COMPILED=""
for i in get-uintX-types.c libdc42-gpl-license.txt libdc42-lgpl-license.txt ./src/libdc42.c hdr/libdc42.h
do
 if [ ! -f ./$i ]
 then
   echo Could not find $i
   echo
   echo "Please run this script from the top level directory. i.e."
   echo
   echo "tar xjpvf libdc42-${VERSION}.tar.bz2"
   echo "cd libdc42-${VERSION}"
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
echo "           libdc42 ${VERSION}  -    Unified Build Script"
echo
echo '                http://lisaem.sunder.net'
echo '    Copyright (C) 2009 Ray Arachelian, All Rights Reserved'
echo 'Released under the terms of the GNU General Public License 2.0'
echo '     or the terms of the LGPL version 2.0 - your choice.    '
echo '----------------------------------------------------------------'

fi


# Parse command line options if any, overriding defaults.
#echo parsing options
for i in $@
do

 case "$i" in
  clean)
            echo "* Removing libdc42 objs"
            rm -f .last-opts last-opts
            cd ./lib  && /bin/rm -f *.a *.o *.dylib *.so
            cd ../obj && /bin/rm -f *.a *.o get-uintX-types*
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


 --32)                         SIXTYFOURBITS=""                          ;;
 --64)                         SIXTYFOURBITS="--64"                      ;;

 --without-debug)              WITHDEBUG=""
                               WARNINGS=""                               ;;

 --with-debug)                 WITHDEBUG="$WITHDEBUG -g"
                               WARNINGS="-Wall"                          ;;
 --with-ipc-comments)          WITHDEBUG="$WITHDEBUG -g -DIPC_COMMENTS"
                               WARNINGS="-Wall"                          ;;
 --with-reg-ipc-comments)      WITHDEBUG="$WITHDEBUG -g -DIPC_COMMENTS -DIPC_COMMENT_REGS"
                               WARNINGS="-Wall"                          ;;


 --with-debug-mem)             WITHDEBUG="$WITHDEBUG -g -DDEBUGMEMCALLS"
                               WARNINGS="-Wall"                          ;;

 --with-profile)               WITHDEBUG="$WITHDEBUG -p"                 ;;

 --without-optimize)           WITHOPTIMIZE=""                           ;;
 --no-68kflag-optimize)        WITH68KFLAGS=""                           ;;


 --with-tracelog)              WITHTRACE="-DDEBUG -DTRACE"
                               WARNINGS="-Wall"                          ;;

 --enable-unicode|--unicode|--no-ansi|--disable-ansi|--without-ansi|--with-unicode)
                           UNICODE="--unicode"
                           WITHUNICODE="--unicode=yes"                       ;;

 --no-unicode|--disable-unicode|--ansi|--enable-ansi|--with-ansi|--without-unicode)
                           UNICODE=""
                           WITHUNICODE="--unicode=no"                        ;;



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
--with-debug-mem          Enable memory call debugging
--with-ipc-comments       Enable IPC comments facility
--with-reg-ipc-comments   Enable IPC comments and register recording
--with-tracelog           Enable tracelog debugging

Other Options:
--without-optimize        Disables optimizations
--no-68kflag-optimize     Force flag calculation on every opcode
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
 # many thanks to David Cecchin for finding the unicode issues fixed below.

 WXCONFIGFLAGS=`wx-config $WITHUNICODE --cppflags`
#if [ -z "$WXCONFIGFLAGS" ]
#then
#   echo wx-config has failed, or returned an error.  Ensure that it exists in your path.
#   which wx-config
#   exit 3
#fi
 CFLAGS="-I. -I../hdr -I../cpu68k -I../wxui $WXCONFIGFLAGS $WITHOPTIMIZE $WITHDEBUG"
 CXXFLAGS="-I. -I../hdr -I../cpu68k -I../wxui $WXCONFIGFLAGS $WITHOPTIMIZE $WITHDEBUG"
 LINKOPTS="`wx-config $STATIC  $WITHUNICODE  --libs --linkdeps --cppflags`"
#if [ -z "$LINKOPTS" ]
#then
#   echo wx-config has failed, or returned an error for link opts.  Ensure that it exists in your path.
#   which wx-config
#   exit 3
#fi

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
   cd ./lib       && /bin/rm -f *.a *.o
   cd ../obj      && /bin/rm -f *.a *.o
   cd ..

fi

echo "LASTTRACE=\"$WITHTRACE\""  > .last-opts
echo "LASTDEBUG=\"$WITHDEBUG\""  >>.last-opts
echo "LASTBLITS=\"$WITHBLITS\""  >>.last-opts
echo "LASTMACHINE=\"$MACHINE\""  >>.last-opts

###########################################################################
echo Building libdc42...
echo

[ ! -f ./hdr/machine.h ] && needclean=1

if [ "$needclean" -gt 0 ]
then

 $CC get-uintX-types.c -o ./obj/get-uintX-types || exit 1
 cd ./hdr
 ../obj/get-uintX-types
 if [ "$?" -ne 0 ]; then
    echo something went wrong finding type information.
    exit 1
 fi
cd ..
fi


if [ -n "$DARWIN" ]
then
   if [ -n "$SIXTYFOURBITS" ]
   then
     CFLAGS="$CFLAGS -arch x86_64 -m64"
     CPPFLAGS="$CFLAGS -arch x86_64 -m64"
     CXXFLAGS="$CXXFLAGS -arch x86_64 -m64"
   else
     CFLAGS="$CFLAGS -arch i386 -m32"
     CPPFLAGS="$CFLAGS -arch i386 -m32"
     CXXFLAGS="$CXXFLAGS -arch i386 -m32"
   fi
fi

     # CODEKARMA FIXME FIXME silence warnings for demos
     CFLAGS="$CFLAGS                -Wno-empty-body  -Wno-duplicate-decl-specifier  -Wno-constant-logical-operand  -Wno-incompatible-pointer-types  -Wno-implicit-function-declaration   -Wno-enum-conversion   -Wno-unsequenced   -Wno-parentheses  -Wno-tautological-constant-out-of-range-compare  -Wno-format -Wno-implicit-function-declaration  -Wno-unused-parameter  -Wno-unused -Wno-bitfield-constant-conversion"


cd src

# only one file, but keeping the structure consistent with the other build.sh files.
for i in libdc42
do

  LIST="$LIST ../obj/$i.o"

  DEPS=0
  [ "$DEPS" -eq 0 ] && if NEEDED ${i}.c             ../obj/${i}.o;then DEPS=1; fi
  if [ "$DEPS" -gt 0 ]
  then
     echo "  Compiling ${i}.c..."
     $CC -W $WARNINGS -fPIC -Wstrict-prototypes -I./hdr -Wno-format -Wno-unused  $WITHDEBUG $WITHTRACE $CFLAGS -c ${i}.c -o ../obj/${i}.o || exit 1
     COMPILED="yes"
  fi
done

DEPS=0
if [ -n "$COMPILED" ]
then
   echo "  Converting to library..."
   MAKELIBS  ../lib libdc42 "${VERSION}" both "$LIST"
fi
cd ..

###########################################################################

if [ -n "$INSTALL" ]
    then
      cd ./lib/
      echo Installing libGenerator $VERSION into $PREFIX/lib from $(pwd)
      mkdir -pm755 $PREFIX/lib
      tar cpf - libdc42* | (cd $PREFIX/lib/; tar xpf - )
      #[ -n "$DARWIN" ] && cp libdc42.${VERSION}.dylib $PREFIX/lib/
      #cd $PREFIX/lib
      #ln -s libdc42-$VERSION.a libdc42.a
    fi
echo
[ -z "$NOBANNER" ] && echo Build Done.
exit 0
