# Makefile

# List of files to be compiled into the application [CHANGE THESE IF NEEDED]
#LMDS_FILES=setup_main.o dac_ad9739.o adc_ev10aq190.o util.o rawcaplmds.o \
#            siggenoptus.o fft.o dac_data_timing.o
LFAA_SIM_FILES=main.o bigfile.o lfaa_tx_data.o

SRCS= $(subst .o,.cpp,$(LFAA_SIM_FILES))

# Names of executables/libraries to be built
TARGETS=lfaa_sim

# Standard options for compile/link/assemble (fPIC needed for library building)
#CPP_COMP_OPTS = -c -g -O2 -Wall --std=c++0x -DOLD_HARDWARE
#C_COMP_OPTS = -c -g -O2 -Wall -DOLD_HARDWARE
CPP_COMP_OPTS = -c -g -O2 -Wall --std=c++14
C_COMP_OPTS = -c -g -O2 -Wall

INCPATHS = -I.
LIBPATHS = -L.

# Compile rules 
.SUFFIXES : .c .cpp .o 

.cpp.o:
	 g++ $(CPP_COMP_OPTS) $(INCPATHS) $<

.c.o:
	 gcc $(C_COMP_OPTS) $(INCPATHS) $<

.phoney: all
.phoney: clean
.phoney: .depend

#Targets to be built (The first one listed is built by default)
all: $(TARGETS) 

clean:
	rm -f *.o *.dbg_o .depend  $(TARGETS)

# recipes for linking each executable in the TARGETS list
lfaa_sim: $(LFAA_SIM_FILES) Makefile
	g++ -o $@ $(LFAA_SIM_FILES) $(LIBPATHS)

# for auto-generation of header dependencies
depend:.depend

.depend: $(SRCS)
	rm -f ./.depend
	g++ $(CPP_COMP_OPTS) $(INCPATHS) -MM $^>>./.depend;


# dependencies of the files in the application
include .depend
