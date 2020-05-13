all: U Q

U:
	gcc -o U2 U2.c -pthread -Wall -Werror
Q:	
	gcc -o Q2 Q2.c -pthread -Wall -Werror

clean:
	rm U2 && rm Q2 && rm door
