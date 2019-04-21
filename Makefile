all: nfind_macOS.c
	gcc -g -Wall -o nfind nfind_macOS.c

clean:
	$(RM) nfind