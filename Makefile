# Project Name
TARGET = 8-step-sequencer

# Sources
CPP_SOURCES = 8-step-sequencer.cpp

# Library Locations
LIBDAISY_DIR = ../../daisy-examples/libDaisy/
DAISYSP_DIR = ../../daisy-examples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
