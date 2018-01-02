# This is a fake Makefile.  For build environments
# that require ./configure && make.  You really
# should use the build.sh script directly.
build:   src/tools/src/lisafsh-tool.c
	./build.sh build 

clean:   src/tools/src/lisafsh-tool.c
	./build.sh clean

install: src/tools/src/lisafsh-tool.c
	./build.sh build  --install

package: src/tools/src/lisafsh-tool.c
	./build.sh build  --package
me:
	
a:
	
sandwich: me a 
	[ `id -u` -ne 0 ] && echo "What? Make it yourself." || echo Okay

love:
	#    "Not war?"

war:
	#    "Not love?"
