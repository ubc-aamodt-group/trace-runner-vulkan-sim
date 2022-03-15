#
# 'make depend' uses makedepend to automatically generate dependencies 
#               (dependencies are added to end of Makefile)
# 'make'        build executable file 'mycc'
# 'make clean'  removes all .o and executable files
#

MESA_PATH := ../mesa
VULKAN_PATHS += -I$(MESA_PATH)/src/intel/
VULKAN_PATHS += -I$(MESA_PATH)/include/
VULKAN_PATHS += -I$(MESA_PATH)/src/
VULKAN_PATHS += -I$(MESA_PATH)/src/gallium/include/
VULKAN_PATHS += -I$(MESA_PATH)/src/mesa
VULKAN_PATHS += -I$(MESA_PATH)/src/mapi/
VULKAN_PATHS += -I$(MESA_PATH)/src/vulkan/util/
VULKAN_PATHS += -I$(MESA_PATH)/build/src/intel/vulkan/
VULKAN_PATHS += -I$(MESA_PATH)/src/vulkan/wsi/
#VULKAN_PATHS += -I$(MESA_PATH)/src/intel/vulkan/
#VULKAN_PATHS += -I$(MESA_PATH)/src/util/

# define the C compiler to use
CC = g++

# define any compile-time flags
CFLAGS = -fpermissive -g3 -Wall  -Wno-unused-function -Wno-sign-compare $(VULKAN_PATHS) -std=c++17 -fPIC
#CFLAGS = -fpermissive -g3 -Wall  -Wno-unused-function -Wno-sign-compare $(VULKAN_PATHS) -std=c++0x -fPIC

#CFLAGS = -Wall -g3 -std=c++0x -Wl,--unresolved-symbols=ignore-in-shared-libs -Wno-unused-function -Wno-sign-compare $(VULKAN_PATHS) -fpermissive -fPIC

# define any directories containing header files other than /usr/include
INCLUDES = -I/usr/include $(VULKAN_PATHS)

# define library paths in addition to /usr/lib
#   if I wanted to include libraries not in /usr/lib I'd specify
#   their path using -Lpath, something like:
GPGPUSIM_PATH = /home/tommy/vulkan_gpgpusim/gpgpu-sim_emerald/lib/gcc-9.4.0/cuda-10010/debug
LFLAGS = -L$(GPGPUSIM_PATH)

# define any libraries to link into executable:
#   if I want to link in libraries (libx.so or libx.a) I use the -llibname 
#   option, something like (this will link in libmylib.so and libm.so:
LIBS = -lcudart -lboost_filesystem

# define the C source files
SRCS = vulkan_rt_runner.cpp

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS = $(SRCS:.cpp=.o)

# define the executable file 
MAIN = vulkan_rt_runner

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

all:    $(MAIN)
		@echo  "*** vulkan_rt_runner has been compiled ***"

$(MAIN): $(OBJS) 
		$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.cpp.o:
		$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
		$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
		makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
