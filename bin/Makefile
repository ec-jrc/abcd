CC = gcc
CXX = g++

CFLAGS = -std=gnu99 -O3 -W -Wall -pedantic -I../include
CXXFLAGS = -O3 -W -Wall -pedantic -I../include

LIBS = -lc
LDFLAGS = $(LIBS)

CSOURCES = sort_ade.c
CXXSOURCES = filter_timestamps.cpp events_counter.cpp
EXECS = $(CSOURCES:.c=) $(CXXSOURCES:.cpp=)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

%: %.cpp
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
