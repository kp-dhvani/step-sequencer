# Project Name
TARGET = pod_patch
LDFLAGS += -u _printf_float
# Sources
CPP_SOURCES = pod_patch.cpp

# Library Locations
LIBDAISY_DIR = ../../../daisy-examples/libDaisy/
DAISYSP_DIR = ../../../daisy-examples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
