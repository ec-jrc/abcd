include ../common_definitions.mk

# File names
EXEC = waan

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

ANALYSIS_SOURCES = $(SRC_DIR)/libNull.c $(SRC_DIR)/libCFD.c $(SRC_DIR)/libTFACFD.c $(SRC_DIR)/libStpAvg.c $(SRC_DIR)/libPSD.c $(SRC_DIR)/libTPZ.c $(SRC_DIR)/libCRRC4.c $(SRC_DIR)/libGeP.c
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
	$(CC) -shared -fPIC $(CFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS) $(ANALYSIS_OBJECTS)
