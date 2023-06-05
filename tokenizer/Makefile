CC:=gcc
CFLAGS:=-Wall -Wextra -Wno-unused-parameter -O2
BINARIES:=parser_labels parser_artists parser_releases

all: $(BINARIES)

yxml.o: yxml.c
	$(CC) $(CFLAGS) -I. yxml.c -c

parser_labels: yxml.o main.c
	$(CC) $(CFLAGS) -DLABELS main.c yxml.o -lz -o parser_labels

parser_artists: yxml.o main.c
	$(CC) $(CFLAGS) -DARTISTS main.c yxml.o -lz -o parser_artists

parser_releases: yxml.o main.c
	$(CC) $(CFLAGS) -DRELEASES main.c yxml.o -lz -o parser_releases

clean:
	rm -f *.o parser*

.PHONY: clean all test
