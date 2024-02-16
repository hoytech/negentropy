# Build a shared library of negentropy

#TODO: Need to add compilation flags based on OS
INCS	= -I./ -I/opt/homebrew/include/ 
TARGET  = libnegentropy.so

.PHONY: all clean install-deps

all: negentropy-cpp c-wrapper shared-lib 

#TODO: Need to add compilation flags based on OS
install-deps:
	brew install lmdb openssl

shared-lib: negentropy.o
	g++ -shared -fPIC -o $(TARGET) negentropy.o negentropy_wrapper.o 

negentropy-cpp:
	g++ negentropy.h --std=c++20 -o negentropy.o $(INCS)

c-wrapper:
	g++ negentropy_wrapper.h --std=c++20 -o negentropy_wrapper.o $(INCS) -I./vendor/lmdbxx/include/

clean:
	rm -f negentropy.o  negentropy_wrapper.o libnegentropy.so
