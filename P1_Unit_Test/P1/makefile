Copy : Copy.c 
	gcc -g Copy.c -o Copy

run:
	-rm -f sample_data1.out1 Copy
	gcc -g Copy.c -o Copy
	./Copy sample/sample_data.in sample_data1.out1 4096

run2:
	-rm -f sample_data1.out2 Copy2
	gcc -g Copy2.c -o Copy2
	./Copy2 sample/sample_data.in sample_data1.out2 4096

clean:
	-rm -f sample_data1.out Copy