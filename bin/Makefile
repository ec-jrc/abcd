include ../common_definitions.mk

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
