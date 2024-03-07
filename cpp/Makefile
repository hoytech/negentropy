# Build a shared library of negentropy

#TODO: Need to add compilation flags based on OS
INCS	= -I./ -I/opt/homebrew/include/ -I./vendor/lmdbxx/include/
TARGET  = libnegentropy.so

.PHONY: all clean install-deps precompiled-header shared-lib

all: precompiled-header shared-lib

#TODO: Need to add compilation flags based on OS
install-deps:
	brew install lmdb openssl

# Generate 'negentropy.h.gch'
precompiled-header:
	g++ -O0 --std=c++20 -Wall -fexceptions -g negentropy.h $(INCS)

shared-lib:
	g++ -O0 -g -std=c++20 $(INCS) -shared -fPIC -o $(TARGET) negentropy_wrapper.c -lcrypto -lssl -L/opt/homebrew/lib/

clean:
	rm -f $(TARGET) negentropy.h.gch libnegentropy.so
