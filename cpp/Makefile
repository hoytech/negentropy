# Build a shared library of negentropy

#TODO: Need to add compilation flags based on OS
INCS	= -I./ -I/opt/homebrew/include/ 
TARGET  = libnegentropy.so

.PHONY: all clean install-deps

all: precompiled-header shared-lib

#TODO: Need to add compilation flags based on OS
install-deps:
	brew install lmdb openssl

# Generate 'negentropy.h.gch'
precompiled-header:
	g++ --std=c++20 -Wall -fexceptions -g negentropy.h -I./ -I./vendor/lmdbxx/include/

shared-lib: negentropy.h.gch
	g++ --std=c++20 -I./ -I./vendor/lmdbxx/include/ -shared -fPIC -o $(TARGET) negentropy_wrapper.c

clean:
	rm -f $(TARGET) negentropy.h.gch libnegentropy.so
