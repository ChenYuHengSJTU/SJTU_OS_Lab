#!/usr/bin/bash

rm -f multhread5
gcc multhread4.c -o multhread4 -lpthread
./multhread4
diff -b -a sample_data_m.out P1/sample/sample_data.out