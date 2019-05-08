FIRE_CXX ?= clang++
FIRE_CXXFLAGS ?= -O3 -std=gnu++2a -Wall -Werror
FIRE_LDLIBS ?= -lgflags -lglog -lpthread

all: firecgi.a example_simple

objects = firecgi.o connection.o request.o parse.o

_firebuf:
	$(MAKE) --directory=firebuf

firecgi.a: $(objects) _firebuf
	ar x firebuf/firebuf.a
	ar rcs $@ $(objects) $(shell ar t firebuf/firebuf.a)

example_simple: example_simple.o firecgi.a
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -o $@ $+ $(FIRE_LDLIBS)

%.o: %.cc *.h Makefile
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -c -o $@ $<

clean:
	$(MAKE) --directory=firebuf clean
	rm --force example_simple connection_afl *.o *.a

afl:
	$(MAKE) clean
	FIRE_CXX=afl-g++ $(MAKE) afl_int

afl_int: connection_afl

connection_afl: connection_afl.o firecgi.a
	$(MAKE) --directory=firebuf
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -o $@ $+ $(FIRE_LDLIBS)

test: test_connection

test_connection: connection_afl
	@echo "Running $$(ls testcases | wc -l) tests"
	for FILE in testcases/*; do ./connection_afl < $$FILE; done
	@printf '\033[0;32mALL TESTS PASSED\033[0m\n'

asan:
	$(MAKE) clean
	FIRE_CXXFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -std=gnu++2a -Wall -Werror" $(MAKE) all
