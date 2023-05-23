all: disk fs client

disk: disk.c 
	gcc -O2 -g disk.c -o disk 

fs: fs.c
	gcc -O2 -g fs.c -o fs

client: client.c
	gcc -O2 -g client.c -o client


clean:
	-rm -rf disk fs client