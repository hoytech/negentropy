W        = -Wall
OPT      = -g -O2
STD      = -std=c++20
CXXFLAGS = $(STD) $(OPT) $(W) -fPIC $(XCXXFLAGS)
INCS     = -I../../cpp/ -I./hoytech-cpp/ -I../cpp/vendor/lmdbxx/include/

DEPS = ../../cpp/negentropy.h ../../cpp/negentropy/* ../../cpp/negentropy/storage/* ../../cpp/negentropy/storage/btree/*

harness: harness.cpp
	$(CXX) $(W) $(OPT) $(STD) $(INCS) $< -lcrypto -o $@

btreeFuzz: btreeFuzz.cpp
	$(CXX) $(W) $(OPT) $(STD) $(INCS) $< -lcrypto -llmdb -o $@

lmdbTest: lmdbTest.cpp
	$(CXX) $(W) $(OPT) $(STD) $(INCS) $< -lcrypto -llmdb -o $@

measureSpaceUsage: measureSpaceUsage.cpp
	$(CXX) -DNE_FUZZ_TEST $(W) $(OPT) $(STD) $(INCS) $< -lcrypto -llmdb -o $@

subRange: subRange.cpp
	$(CXX) -DNE_FUZZ_TEST $(W) $(OPT) $(STD) $(INCS) $< -lcrypto -o $@


.PHONY: all clean

all: harness btreeFuzz lmdbTest measureSpaceUsage subRange

clean:
	rm -f harness btreeFuzz lmdbTest measureSpaceUsage
