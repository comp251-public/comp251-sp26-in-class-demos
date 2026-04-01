CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Wno-unused-variable -Wno-unused-parameter

# Shared object files
SHARED_OBJS = list.o

all: display_route list_test

display_route: display_route.o intersect.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

list_test: list_test.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

display_route.o: display_route.c intersect.h list.h
	$(CC) $(CFLAGS) -c $<

intersect.o: intersect.c intersect.h
	$(CC) $(CFLAGS) -c $<

list_test.o: list_test.c list.h
	$(CC) $(CFLAGS) -c $<

list.o: list.c list.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o display_route list_test
