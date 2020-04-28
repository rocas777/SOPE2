all: U Q

U:
	gcc -o U U.c -pthread -Wall -Werror

Q:	
	gcc -o Q Q.c -pthread -Wall -Werror

clean:
	rm U && rm Q
