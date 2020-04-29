all: U Q

U:
	gcc -o U U.c -pthread -Wall 
Q:	
	gcc -o Q Q.c -pthread -Wall 

clean:
	rm U && rm Q
