test : test.o log.o
	gcc -g  -o test test.o log.o `pkg-config fuse --libs`

test.o : test.c log.h test.h
	gcc -g -Wall `pkg-config fuse --cflags` -c test.c 

log.o : log.c log.h test.h
	gcc -g -Wall `pkg-config fuse --cflags` -c log.c

clean:
	rm -f test *.o

