CC = gcc
CXX = g++
CCFLAGS = -Wall -Wextra -pedantic
CXXFLAGS = -std=c++14

LEDPI_OBJS = main.o pigpio.o Uv.o command.capnp.o state.capnp.o
LEDCTL_OBJS = control.o Uv.o command.capnp.o

all: ledpi ledctl

ledpi: $(LEDPI_OBJS)
	$(CXX) -o $@ $(LEDPI_OBJS) -luv -lcapnp -lkj

ledctl: $(LEDCTL_OBJS)
	$(CXX) -o $@ $(LEDCTL_OBJS) -luv -lcapnp -lkj

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)

%.o: %.c
	$(CC) -c -o $*.o $*.c
	$(CC) -MM -c -o $*.d $*.c

%.o: %.cpp | generated_headers
	$(CXX) -c -o $*.o $*.cpp $(CCFLAGS) $(CXXFLAGS)
	$(CXX) -MM -c -o $*.d $*.cpp $(CCFLAGS) $(CXXFLAGS)

%.o: %.c++
	$(CXX) -c -o $*.o $*.c++ $(CCFLAGS) $(CXXFLAGS)

%.capnp:
	@:

%.capnp.c++ %.capnp.h: %.capnp
	capnpc -oc++ $<

generated_headers: command.capnp.h state.capnp.h

clean:
	rm -f ledpi ledctl *.o *.d *.capnp.c++ *.capnp.h

.PHONY: all clean generated_headers
