all : LCM Faneuil

LCM : LarryCurlyMoe.c 
	gcc -O2 -g -Werror LarryCurlyMoe.c -o LCM -lpthread

Faneuil : Faneuil.c 
	gcc -O2 -g -Werror Faneuil.c -o Faneuil -lpthread

clean : 
	-rm -rf LCM Faneuil