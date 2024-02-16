# Build a shared library of negentropy

#TODO: Need to add compilation flags based on OS
INCS	= -I./ -I/opt/homebrew/include/
TARGET  = libnegentropy.so

.PHONY: all clean

all: negentropy-cpp shared-lib

shared-lib: negentropy.o
	g++ -o $(TARGET) negentropy.o -shared

negentropy-cpp:
	g++ negentropy.h --std=c++20 -o negentropy.o $(INCS)


clean: 
	rm -f negentropy.o