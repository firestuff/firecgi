FIRE_CXX ?= clang++
FIRE_CXXFLAGS ?= -O3 -std=gnu++2a -Wall -Werror -Wextra -fPIE -fPIC -fstack-protector-strong -fsanitize=safe-stack -fsanitize=safe-stack
FIRE_LDFLAGS ?= -fuse-ld=gold -flto -Wl,-z,relro -Wl,-z,now
FIRE_LDLIBS ?= -lgflags -lglog -lpthread

all: firecgi.a firecgi.o firecgi.so example_simple

objects = server.o connection.o request.o parse.o

firebuf/firebuf.o:
	$(MAKE) --directory=firebuf firebuf.o

fireusage/fireusage.o:
	$(MAKE) --directory=fireusage fireusage.o

firecgi.a: $(objects)
	ar rcs $@ $^

firecgi.o: $(objects) firebuf/firebuf.o fireusage/fireusage.o
	gold -z relro -z now -r --output=$@ $+

firecgi.so: $(objects) firebuf/firebuf.o fireusage/fireusage.o
	$(FIRE_CXX) $(FIRE_CXXFLAGS) $(FIRE_LDFLAGS) -shared -o $@ $+ $(FIRE_LDLIBS)

example_simple: example_simple.o firecgi.o
	$(FIRE_CXX) $(FIRE_CXXFLAGS) $(FIRE_LDFLAGS) -pie -o $@ $+ $(FIRE_LDLIBS)

%.o: %.cc *.h Makefile
	$(FIRE_CXX) $(FIRE_CXXFLAGS) -c -o $@ $<

clean:
	$(MAKE) --directory=firebuf clean
	$(MAKE) --directory=fireusage clean
	rm --force example_simple connection_afl *.so *.o *.a

afl:
	$(MAKE) clean
	FIRE_CXX=afl-g++ FIRE_CXXFLAGS="-O3 -std=gnu++2a -fPIC -fPIE" $(MAKE) afl_int

afl_int: connection_afl

connection_afl: connection_afl.o firecgi.o
	$(FIRE_CXX) $(FIRE_CXXFLAGS) $(FIRE_LDFLAGS) -pie -o $@ $+ $(FIRE_LDLIBS)

test: test_connection

test_connection: connection_afl
	@echo "Running $$(ls testcases | wc -l) tests"
	for FILE in testcases/*; do ./connection_afl < $$FILE; done
	@printf '\033[0;32mALL TESTS PASSED\033[0m\n'

asan:
	$(MAKE) clean
	FIRE_CXXFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -std=gnu++2a -fPIE -fPIC" $(MAKE) all

tsan:
	$(MAKE) clean
	FIRE_CXXFLAGS="-O1 -g -fsanitize=thread -std=gnu++2a -fPIE -fPIC" $(MAKE) all

ubsan:
	$(MAKE) clean
	FIRE_CXXFLAGS="-O1 -g -fsanitize=undefined -std=gnu++2a -fPIE -fPIC" $(MAKE) all
