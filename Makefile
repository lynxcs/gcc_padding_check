C = gcc
CXX = g++
# Flags for the C++ compiler: enable C++11 and all the warnings, -fno-rtti is required for GCC plugins
CXXFLAGS = -std=c++23 -Wall -fno-rtti -g -O3
# Workaround for an issue of -std=c++11 and the current GCC headers
CXXFLAGS += -Wno-literal-suffix -Wno-format-diag

# Determine the plugin-dir and add it to the flags
PLUGINDIR=$(shell $(CXX) -print-file-name=plugin)
CXXFLAGS += -I$(PLUGINDIR)/include

# top level goal: build our plugin as a shared library
all: padding_check_cxx.so padding_check_c.so

padding_check_c.so: padding_check_c.o
	$(CXX) $(LDFLAGS) -shared -o $@ $<

padding_check_cxx.so: padding_check_cxx.o
	$(CXX) $(LDFLAGS) -shared -o $@ $<

padding_check_c.o : padding_check.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $<

padding_check_cxx.o : padding_check.cpp
	$(CXX) $(CXXFLAGS) -DUSE_CXX_PRINT -fPIC -c -o $@ $<

clean:
	rm -f padding_check_c.o padding_check_c.so padding_check_cxx.o padding_check_cxx.so

check_cpp: padding_check_cxx.so test/test.cc
	$(CXX) -fplugin=./padding_check_cxx.so -c test/test.cc -I test/exclude_dir -I test/inc_dir -isystem test/sys_dir -o /dev/null

check_c: padding_check_c.so test/test.c
	$(C) -fplugin=./padding_check_c.so -c test/test.c -I test/exclude_dir -I test/inc_dir -isystem test/sys_dir -o /dev/null

check: check_c check_cpp

.PHONY: all clean check
