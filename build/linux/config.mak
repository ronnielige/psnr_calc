# build options
# 1. build .so for arcvideo: make BUILD=release
# 2. build cli:              make cli BUILD=release

COLOR=yes

#debug release release_withtrace
BUILD=release

#NOTE: must be dynamic in this project
LIBTYPE=dynamic

#use intel compiler
ICC=no

# user specific information
LIB_FILE_NAME := quality_metric

CLI_FILE_NAME := quality_metric


# general directory
PREFIX      := /usr/local/
BIN_DIR     := ${PREFIX}bin/
LIB_DIR     := ${PREFIX}lib/
INCLUDE_DIR := ${PREFIX}include/

# general file extentions
LIBSUFFIX := a
SOSUFFIX  := so
EXESUFFIX := exe
YUVSUFFIX := yuv
LOGSUFFIX := log

# general configure
ARCH=X86_64
SYS=LINUX
#linux
#CC=color-gcc
ifeq ($(ICC), yes)
  CC=/opt/intel/bin/icpc
else
  CC=/usr/bin/g++
endif

#CXX=color-gcc
ifeq ($(ICC), yes)
  CXX=/opt/intel/bin/icpc
else
  CXX=/usr/bin/g++
endif

#LD=gcc
ifeq ($(ICC), yes)
  LD=/opt/intel/bin/icpc
else
  LD=/usr/bin/g++
endif

ifeq ($(DEBUGLOG), yes)
  DEBUG_LOG_FLAG = -DDEBUGLOG
else
  DEBUG_LOG_FLAG = 
endif

#Folder
Fcli = ../../src
Fsourcecode = ../../
Fasm = $(Fsourcecode)/asm
Finc = $(Fsourcecode)/inc
Fsrc = $(Fsourcecode)/src

ifeq ($(BUILD), debug)
  Fdst := debug
else
  Fdst := release
endif

# includes
CINCLUDES := -I. -I../lib -I $(Fsourcecode) -I $(Finc) -I $(Fasm)
             
CXXINCLUDES  := $(CINCLUDES)
LINKINCLUDES := -L. -L$(LIB_DIR)
EXEC_TEST_LIBS := # -lboost_test_exec_monitor
UNIT_TEST_LIBS := -lboost_unit_test_framework  -lm -lstdc++

# user specific flags
ifeq ($(TRACE), yes)
  TRACEFLAGS = -DTRACE
else
  TRACEFLAGS = -DNOTHING
endif

ifeq ($(LIBTYPE), static)
  CLI_LIB = $(LIBFULLNAME)
else
  CLI_LIB = $(SOFULLNAME)
endif

USER_SHARED_COMPILEFLAGS := -Dlinux -pipe -fomit-frame-pointer -fpermissive -W -Wall -Wno-write-strings -shared -fPIC
USER_DEBUG_COMPILEFLAGS := -g -O0
USER_RELEASE_COMPILEFLAGS := -O3

ifeq ($(ICC), yes)
  USER_SHARED_COMPILEFLAGS := $(USER_SHARED_COMPILEFLAGS) -msse4
else
  USER_SHARED_COMPILEFLAGS := $(USER_SHARED_COMPILEFLAGS) -mmmx -msse -msse2 -msse3 -msse4
endif

ifeq ($(BUILD),debug)
  USER_OPTIMIZEFLAGS := $(USER_SHARED_COMPILEFLAGS) $(USER_DEBUG_COMPILEFLAGS)   $(DEBUG_LOG_FLAG)
else
  USER_OPTIMIZEFLAGS := $(USER_SHARED_COMPILEFLAGS) $(USER_RELEASE_COMPILEFLAGS) $(DEBUG_LOG_FLAG)
endif

# gcov related
ifeq (1,${TEST-COVERAGE})
  GCOVFLAGS  := --coverage
else
  GCOVFLAGS  :=
endif

# c related
CFLAGS   := $(CINCLUDES) $(USER_OPTIMIZEFLAGS) $(GCOVFLAGS)

# c++ related
CXXFLAGS := $(CXXINCLUDES) $(USER_OPTIMIZEFLAGS) $(GCOVFLAGS)

# asm related
AS       =yasm
ASFLAGS  := -f elf64 -m amd64 -DARCH_X86_64=1 -DPIC -DHAVE_ALIGNED_STACK=1
ifeq ($(BITDEPTH), 10)
ASFLAGS  += -DBIT_DEPTH=10
else
ASFLAGS  += -DBIT_DEPTH=8
endif

ifeq ($(BUILD),debug)
ASFLAGS  += -g stabs
endif

USER_SHARED_LDFLAGS := -L. -L../../lib -lstdc++  -lpthread -Wl,--rpath=./ -Wl,--retain-symbols-file=retain_symbols.txt -Wl,-version-script=version-script.txt
ifeq ($(ICC), yes)
  USER_SHARED_LDFLAGS := -static-intel -wd10237 $(USER_SHARED_LDFLAGS) 
endif

USER_DEBUG_LDFLAGS :=
USER_RELEASE_LDFLAGS :=
ifeq ($(BUILD),debug)
  USER_LDFLAGS := $(USER_SHARED_LDFLAGS) $(USER_DEBUG_LDFLAGS) -Wl,--rpath=$(CURDIR)/../../../../bin/linux/debug
else ifeq ($(BUILD), release)
  USER_LDFLAGS := $(USER_SHARED_LDFLAGS) $(USER_RELEASE_LDFLAGS) -Wl,--rpath=$(CURDIR)/../../../../bin/linux/release
endif

LDFLAGS     := -lm -lstdc++ $(USER_LDFLAGS) $(GCOVFLAGS)
LIBNAME     := $(LIB_FILE_NAME).$(LIBSUFFIX)
LIBFULLNAME := $(Fdst)/$(LIBNAME)
AR          =ar rc
RANLIB      =ranlib

# dynamic lib related
#SONAME     := $(LIBNAME:%.a=%.$(SOSUFFIX).$(SOVERSION))
SONAME      := $(LIBNAME:%.a=%.$(SOSUFFIX))
SOFULLNAME  := $(Fdst)/$(LIBNAME:%.a=%.$(SOSUFFIX))
SOLINKNAME  := $(LIBNAME:%.a=%.$(SOSUFFIX))
SOFLAGS     := -shared -Wl,-soname,$(SONAME) $(GCOVFLAGS)

# cli related
CLIFULLNAME := $(Fdst)/$(CLI_FILE_NAME)
#LDFLAGSCLI  := -pthread -lm -lstdc++ $(GCOVFLAGS)
LDFLAGSCLI  := -lm -lstdc++ -ldl $(USER_LDFLAGS) $(GCOVFLAGS)

# colorful build message
ifeq ($(COLOR), yes)
  BUILD_MSG = "\033[33mBuilding $@\033[0m"
else
  BUILD_MSG = "Building $@"
endif

