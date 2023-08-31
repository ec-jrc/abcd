include ../common_definitions.mk

# File names
EXEC = waan

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

ANALYSIS_SOURCES = $(SRC_DIR)/libSimplePSD.c $(SRC_DIR)/libLE.c $(SRC_DIR)/libRT.c $(SRC_DIR)/libCFD.c $(SRC_DIR)/libTFACFD.c $(SRC_DIR)/libStpAvg.c $(SRC_DIR)/libPSD.c $(SRC_DIR)/libTPZ.c $(SRC_DIR)/libCRRC4.c $(SRC_DIR)/libRC4.c $(SRC_DIR)/libRunningMean.c $(SRC_DIR)/libLeftThr.c $(SRC_DIR)/libLogistic.c
ANALYSIS_OBJECTS = $(ANALYSIS_SOURCES:.c=.so)

CFLAGS += 
CXXFLAGS += -I../../api/include -I /opt/redpitaya/include

LIBS = -lc -lzmq -ljansson -lm -ldl -lgsl
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
