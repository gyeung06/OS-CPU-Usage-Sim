
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200112L -Werror

build:
	gcc $(CFLAGS) main.c -o main list.o
	
valgrind: build
	valgrind --leak-check=full ./main 

clean:
	rm -f main

