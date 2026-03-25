CC = gcc
CFLAGS = -Wall -Wextra -g

display_route: display_route.o intersect.o
	$(CC) $(CFLAGS) -o display_route display_route.o intersect.o

display_route.o: display_route.c intersect.h
	$(CC) $(CFLAGS) -c display_route.c

intersect.o: intersect.c intersect.h
	$(CC) $(CFLAGS) -c intersect.c

clean:
	rm -f *.o display_route
