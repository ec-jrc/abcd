include ../common_definitions.mk

# File names
EXEC = waan

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

ANALYSIS_SOURCES = $(wildcard $(SRC_DIR)/*.c)
ANALYSIS_OBJECTS = $(ANALYSIS_SOURCES:.c=.so)

CFLAGS += 
CXXFLAGS += -I../../api/include -I /opt/redpitaya/include

LIBS = -lc -lzmq -ljansson -lm -ldl -lgsl -lgslcblas
LDFLAGS += -L /usr/lib -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXEC) $(ANALYSIS_OBJECTS)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
%.so: %.c
	$(CC) -shared -fPIC $(CFLAGS) $< -o $@ $(LIBS)

# Main target
$(EXEC): $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS) $(ANALYSIS_OBJECTS)
