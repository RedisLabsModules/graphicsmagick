# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?=  -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared
else
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif
CFLAGS = -I/usr/include/GraphicsMagick -Wall -g -fPIC -lc -lm -Og -std=gnu99
CC=gcc
.SUFFIXES: .c .so .xo .o

all: rm_graphicsmagick.so 

#c.xo:
#$(CC) -I. $(CFLAGS) $(SHOBJ_CFLAGS) -fPIC -c $< -o $@

#mdma.xo: ./index.h

rm_graphicsmagick.so: rm_graphicsmagick.o
	$(LD) -o $@ rm_graphicsmagick.o $(SHOBJ_LDFLAGS) $(LIBS) -lGraphicsMagick -lc

clean:
	rm -rf *.xo *.so *.o

