all : Copy shell single multi multi_static multi_random strassen

CFlags = -O2 -g -w

Copy : Copy.c 
	gcc $(CFlags) Copy.c -o Copy

shell : shell.c 
	gcc $(CFlags) shell.c -o shell

multi : multi.c multi_static.c multi_random.c
	gcc $(CFlags) multi.c -o multi
	gcc $(CFlags) multi_static.c -o multi_static -lpthread
	gcc $(CFlags) multi_random.c -o multi_random -lpthread

strassen : strassen.c
	gcc $(CFlags) strassen.c -o strassen -lpthread

clean:
	-rm -f Copy multi shell multi_random multi_static