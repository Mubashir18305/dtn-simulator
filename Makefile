CC = gcc
CFLAGS = -Wall -Wextra -g

all: ground_station sat_a sat_b sat_c test_visibility

ground_station: ground_station.c bundle.c socket_util.c
	$(CC) $(CFLAGS) -o $@ $^

sat_a: sat_a.c bundle.c socket_util.c
	$(CC) $(CFLAGS) -o $@ $^

sat_b: sat_b.c bundle.c socket_util.c
	$(CC) $(CFLAGS) -o $@ $^

sat_c: sat_c.c bundle.c socket_util.c
	$(CC) $(CFLAGS) -o $@ $^

test_visibility: test_visibility.c bundle.c socket_util.c
	$(CC) $(CFLAGS) -o $@ $^

test: test_visibility
	./test_visibility

clean:
	rm -f ground_station sat_a sat_b sat_c test_visibility *.o

.PHONY: all test clean
