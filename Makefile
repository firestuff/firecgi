FIRE_CXX ?= clang++
FIRE_CXXFLAGS ?= -O3 -std=gnu++2a -Wall -Werror
FIRE_LDLIBS ?= -lgflags -lglog -lpthread

all: firecgi.a example_simple

objects = firecgi.o connection.o request.o parse.o

firecgi.a: $(objects)
	$(MAKE) --directory=firebuf
	ar rcs $@ $^

example_simple: example_simple.o $(objects)
	$(MAKE) --directory=firebuf
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -o $@ $+ firebuf/firebuf.a $(FIRE_LDLIBS)

%.o: %.cc *.h Makefile
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -c -o $@ $<

clean:
	$(MAKE) --directory=firebuf clean
	rm --force example_simple connection_afl *.o

afl:
	$(MAKE) clean
	FIRE_CXX=afl-g++ $(MAKE) afl_int

afl_int: connection_afl

connection_afl: connection_afl.o $(objects)
	$(MAKE) --directory=firebuf
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -o $@ $+ firebuf/firebuf.a $(FIRE_LDLIBS)

test: test_connection

test_connection: connection_afl
	@echo "Running $$(ls testcases | wc -l) tests"
	for FILE in testcases/*; do ./connection_afl < $$FILE; done
	@printf '\033[0;32mALL TESTS PASSED\033[0m\n'
