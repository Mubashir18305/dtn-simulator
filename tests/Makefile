CC = gcc
CFLAGS = -Wall -I.. -DTEST_BUILD

test_bundle: test_bundle.c ../bundle.c stubs.c
	$(CC) $(CFLAGS) $^ -o $@

run_tests: test_bundle
	./test_bundle

clean:
	rm -f test_bundle
