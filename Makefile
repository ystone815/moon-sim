# SystemC environment - set SYSTEMC_HOME in your shell environment
# Example: export SYSTEMC_HOME=/usr/local/systemc
ifndef SYSTEMC_HOME
$(error SYSTEMC_HOME is not set. Please set it in your environment: export SYSTEMC_HOME=/path/to/systemc)
endif

CXX=g++
CXXFLAGS=-std=c++11 -I$(SYSTEMC_HOME)/include -Iinclude -Iinclude/base -Iinclude/host_system -Iinclude/ssd -Iinclude/common -Iinclude/packet -L$(SYSTEMC_HOME)/lib-linux64 -lsystemc -Wl,-rpath=$(SYSTEMC_HOME)/lib-linux64

EXE=sim
SSD_EXE=sim_ssd
CACHE_EXE=cache_test
WEB_EXE=web_test
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

# SSD simulation specific files
SRCS_SSD = src/main_ssd.cpp
OBJS_SSD = $(patsubst src/%.cpp, $(OBJ_DIR)/%.o, $(SRCS_SSD))
OBJS_SSD_TOTAL = $(OBJS_SSD) $(OBJS_BASE) $(OBJS_HOST_SYSTEM)

# Cache test specific files
SRCS_CACHE = src/main_cache_test.cpp
OBJS_CACHE = $(patsubst src/%.cpp, $(OBJ_DIR)/%.o, $(SRCS_CACHE))
OBJS_CACHE_TOTAL = $(OBJS_CACHE) $(OBJS_BASE) $(OBJS_HOST_SYSTEM)

# Web test specific files
SRCS_WEB = src/main_web_test.cpp
OBJS_WEB = $(patsubst src/%.cpp, $(OBJ_DIR)/%.o, $(SRCS_WEB))
OBJS_WEB_TOTAL = $(OBJS_WEB) $(OBJS_BASE) $(OBJS_HOST_SYSTEM)

all: $(EXE)

ssd: $(SSD_EXE)

cache_test: $(CACHE_EXE)

web_test: $(WEB_EXE)

$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SSD_EXE): $(OBJS_SSD_TOTAL)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CACHE_EXE): $(OBJS_CACHE_TOTAL)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(WEB_EXE): $(OBJS_WEB_TOTAL)
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
	rm -f $(EXE) $(SSD_EXE) $(CACHE_EXE) $(WEB_EXE) $(OBJS) $(OBJS_SSD) $(OBJS_CACHE) $(OBJS_WEB)
	rm -rf $(OBJ_DIR)
