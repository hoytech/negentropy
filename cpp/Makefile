# Build a shared library of negentropy

# Define the root directory of the negentropy project; this absolute path mechanism works across all major os
NEGENTROPY_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
INCS	= -I$(NEGENTROPY_ROOT) -I/opt/homebrew/include/ -I$(NEGENTROPY_ROOT)/vendor/lmdbxx/include/

ifeq ($(OS),Windows_NT)
	TARGET  = libnegentropy.dll
else
	TARGET  = libnegentropy.so
endif

.PHONY: all clean install-deps precompiled-header shared-lib

all: precompiled-header shared-lib

#TODO: Need to add compilation flags based on OS
install-deps:
	brew install lmdb openssl

# Generate 'negentropy.h.gch'
precompiled-header:
	g++ -O0 --std=c++20 -Wall -fexceptions -g $(NEGENTROPY_ROOT)negentropy.h $(INCS)

shared-lib:
	g++ -O0 -g -std=c++20 $(INCS) -shared -fPIC -o $(TARGET) $(NEGENTROPY_ROOT)negentropy_wrapper.cpp -lcrypto -lssl -L/opt/homebrew/lib/

clean:
	rm -f $(TARGET) negentropy.h.gch libnegentropy.so
