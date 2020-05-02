all: U Q

U:
	gcc -o U1 U1.c -pthread -Wall -Werror
Q:	
	gcc -o Q1 Q1.c -pthread -Wall -Werror

clean:
	rm U1 && rm Q1
