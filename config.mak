#
# config.mk: included by all make files.
#

# Check parameters.(T=v400)

# Make to make a PPC version.
# Make T=x86 to make a X86 version.

# Make web=0 to disable links browser

ifndef T
T=v400
endif

ifeq ("x$(T)", "xv300")
	CROSS_COMPILE = arm-none-linux-gnueabi-
endif
ifeq ("x$(T)", "xv100p")
	CROSS_COMPILE = powerpc-860-linux-gnu-
endif

ifndef INSTALL
INSTALL=install
endif


#compile tools
CP  =   cp -f
AS	=	$(CROSS_COMPILE)as
CC	=	$(CROSS_COMPILE)gcc
CXX =	$(CROSS_COMPILE)g++
LD	=	$(CROSS_COMPILE)ld
AR	=	$(CROSS_COMPILE)ar
STRIP  =	$(CROSS_COMPILE)strip
RANLIB =	$(CROSS_COMPILE)ranlib

COMMON_DEFINITIONS =
COMMON_INCLUDES    = 
COMMON_STATICLIBS  =
COMMON_SHAREDLIBS  =

#FLAGS
CFLAGS  = -fsigned-char

COMMON_LIBRARIES = $(COMMON_STATICLIBS) $(COMMON_SHAREDLIBS)

CFLAGS+= $(COMMON_DEFINITIONS) $(COMMON_INCLUDES)

#only add the following variables for compatible reson, shit!!!
CXXFLAGS = $(CFLAGS)
CPP = $(CXX)

#include directory

#common rules
%.o: %.c %.dep
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp %.dep
	$(CXX) -c $(CXXFLAGS) -o $@ $<
	
%.dep: %.c
	$(CC) $(CFLAGS) -MM $< > $@

%.dep: %.cpp
	$(CXX) $(CXXFLAGS) -MM $< > $@

ifndef SOURCES_FILES_C
SOURCES_FILES_C=$(wildcard *.c) 
endif
ifndef SOURCES_FILES_CPP
SOURCES_FILES_CPP=$(wildcard *.cpp)
endif

SOURCE_FILES=$(SOURCES_FILES_C) $(SOURCES_FILES_CPP)
OBJ_FILES=$(patsubst %.c,%.o,$(SOURCES_FILES_C)) $(patsubst %.cpp,%.o,$(SOURCES_FILES_CPP)) 
DEP_FILES=$(patsubst %.c,%.dep,$(SOURCES_FILES_C)) $(patsubst %.cpp,%.dep,$(SOURCES_FILES_CPP)) 

LINK_INCREMENT=$(RM) $@; $(LD) -o $@ -r $^


.PHONY: prebuild preclean forcecheck dep all 

#you should define your TARGET and prepare in Makefile before include config.mk
all: dep prebuild $(TARGET)

dep: $(DEP_FILES)

show:
	@echo "info: cross-compiler = $(CROSS_COMPILE)"
	@echo "info: sources = $(SOURCE_FILES)"
	@echo "info: objs = $(OBJ_FILES)"
	@echo "info: deps = $(DEP_FILES)"

clean: preclean	
	$(RM) $(OBJ_FILES) $(TARGET) $(DEP_FILES)

ifneq ("x$(CLEAN_FLAG)","xYES")
-include $(DEP_FILES)
endif
