# Build a shared library of negentropy

# Define the root directory of the negentropy project; this absolute path mechanism works across all major os
NEGENTROPY_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
INCS	= -I$(NEGENTROPY_ROOT) -I/opt/homebrew/include/ -I$(NEGENTROPY_ROOT)/vendor/lmdbxx/include/

ifeq ($(OS),Windows_NT)
	TARGET  = libnegentropy.dll
else
	TARGET  = libnegentropy.a
endif

.PHONY: all clean install-deps precompiled-header build-lib

all: precompiled-header build-lib

#TODO: Need to add compilation flags based on OS
install-deps:
	brew install lmdb openssl

# Generate 'negentropy.h.gch'
precompiled-header:
	g++ -x c++-header -O0 --std=c++20 -Wall -fexceptions -g -o $(NEGENTROPY_ROOT)negentropy.h.gch $(NEGENTROPY_ROOT)negentropy.h $(INCS)

build-lib:
ifeq ($(OS),Windows_NT)
	g++ -O0 -g -std=c++20 $(INCS) -shared -fPIC -o $(TARGET) $(NEGENTROPY_ROOT)negentropy_wrapper.cpp -lcrypto -lssl -L/opt/homebrew/lib/
else
	g++ -O0 -g -std=c++20 $(INCS) -fPIC -c negentropy_wrapper.cpp
	ar rcs $(TARGET) negentropy_wrapper.o
endif

clean:
	rm -f $(TARGET) negentropy.h.gch *.o
