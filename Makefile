SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11

CXX=g++
CXXFLAGS=-std=c++11 -I$(SYSTEMC_HOME)/include -Iinclude -Iinclude/base -Iinclude/host_system -Iinclude/common -Iinclude/packet -L$(SYSTEMC_HOME)/lib-linux64 -lsystemc -Wl,-rpath=$(SYSTEMC_HOME)/lib-linux64

EXE=sim
OBJ_DIR=obj

# Define source files in their new locations
SRCS_MAIN = src/main.cpp
SRCS_BASE = src/base/traffic_generator.cpp
SRCS_HOST_SYSTEM = src/host_system/host_system.cpp

# Define object files based on source files and their new locations
OBJS_MAIN = $(patsubst src/%.cpp, $(OBJ_DIR)/%.o, $(SRCS_MAIN))
OBJS_BASE = $(patsubst src/base/%.cpp, $(OBJ_DIR)/base/%.o, $(SRCS_BASE))
OBJS_HOST_SYSTEM = $(patsubst src/host_system/%.cpp, $(OBJ_DIR)/host_system/%.o, $(SRCS_HOST_SYSTEM))

OBJS = $(OBJS_MAIN) $(OBJS_BASE) $(OBJS_HOST_SYSTEM)

all: $(EXE)


$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^


# Rule for main.cpp
$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule for base module source files
$(OBJ_DIR)/base/%.o: src/base/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Rule for host_system module source files
$(OBJ_DIR)/host_system/%.o: src/host_system/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(EXE) $(OBJS)
	rm -rf $(OBJ_DIR)
