harness: harness.cpp ../../cpp/negentropy.h ../../cpp/negentropy/* ../../cpp/negentropy/storage/* ../../cpp/negentropy/storage/btree/*
	g++ -g -Wall -std=c++20 -I../../cpp/ -I ./hoytech-cpp/ harness.cpp -lcrypto -o harness

btreeFuzz: btreeFuzz.cpp ../../cpp/negentropy.h ../../cpp/negentropy/* ../../cpp/negentropy/storage/*
	g++ -DNE_FUZZ_TEST -g -Wall -std=c++20 -I../../cpp/ -I ./hoytech-cpp/ -I lmdbxx/include/ btreeFuzz.cpp -lcrypto -llmdb -o btreeFuzz
