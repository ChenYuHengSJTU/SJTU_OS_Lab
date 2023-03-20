all : Copy shell single multi multi_static multi_random strassen

Copy : Copy.c 
	gcc -O2 -g Copy.c -o Copy

shell : shell.c 
	gcc -O2 -g shell.c -o shell

multi : multi.c multi_static.c multi_random.c
	gcc -O2 -g multi.c -o multi
	gcc -O2 -g multi_static.c -o multi_static -lpthread
	gcc -O2 -g multi_random.c -o multi_random -lpthread

strassen : strassen.c
	gcc -O2 -g strassen.c -o strassen -lpthread

clean:
	-rm -f Copy multi shell multi_random multi_static