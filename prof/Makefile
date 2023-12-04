# joonho
# Makefile for C & C++

CONDA_PREFIX=/scratch/joonho.whangbo/coding/chipyard/.conda-env

# makefile can use variables : use $() to use them
CC=g++
CXXFLAGS += -Wall -O2 # use CFLAGS for C
CXXFLAGS += -g
# LDFLAGS= -L
LIBS=-L$(CONDA_PREFIX)/lib -l:libdwarf.so -l:libelf.so


# < makefile format >
# target : prerequisite
# (tab) recipie

# < makefile automatic variables >
# $< : first entry of prerequisite 
# $^ : all entries of prerequisite
# $@ : target
# $? : prerequisites that are younger 
#      than the target

# 1. first make object files and link them
# foo.o : foo.cc foo.h
# $(CC) $(CXXFLAGS) -c foo.cc

# bar.o : bar.cc bar.h
# $(CC) $(CXXFLAGS) -c bar.cc

# main.o : main.cc foo.h bar.h
# $(CC) $(CXXFLAGS) -c main.cc

# main : foo.o bar.o main.o
# $(CC) $(CXXFLAGS) -o main

# list of object files 
OBJS_FILES = main.o trace_tracker.o tracerv_dwarf.o tracerv_elf.o tracerv_processing.o
OBJ_DIR := obj
OBJS := $(addprefix $(OBJ_DIR)/, $(OBJS_FILES))

SRC_DIR := src

DEPS = $(OBJS:.o=.d)

# 2. simplify !
# % is used like *
# so now we can just merge the compilation of 
# foo.o & bar.o into this

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.cc
	$(CC) $(CXXFLAGS) -o $@ -c -MD $<

main: $(OBJS)
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJS): | $(OBJ_DIR)
$(OBJ_DIR):
	mkdir $(OBJ_DIR)

# in case there is a directory named "clean"
.PHONY : clean
clean:
	rm -rf $(OBJ_DIR) main

-include $(DEPS)
