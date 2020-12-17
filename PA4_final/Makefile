CC=gcc
CFLAGS=-std=c99

default: dfc dfs

dfc:
	$(CC) -o dfc client/dfc.c common.h -lssl -lcrypto

dfs:
	$(CC) -o dfs server/dfs.c common.h -lpthread

clean:
	rm dfs
	rm dfc
