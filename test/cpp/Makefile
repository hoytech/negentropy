harness: harness.cpp ../../cpp/negentropy.h ../../cpp/negentropy/* ../../cpp/negentropy/storage/* ../../cpp/negentropy/storage/btree/*
	g++ -g -Wall -std=c++20 -I../../cpp/ -I ./hoytech-cpp/ harness.cpp -lcrypto -o harness

btree: btree.cpp ../../cpp/negentropy.h ../../cpp/negentropy/* ../../cpp/negentropy/storage/*
	g++ -g -Wall -std=c++20 -I../../cpp/ -I ./hoytech-cpp/ -I lmdbxx/include/ btree.cpp -lcrypto -llmdb -o btree
