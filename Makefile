# acsde Makefile.

EXE=acsde
BUILD_DIR=build
CC=gcc
INCLUDE=-Isrc
OPTIONS=-g -Wall -Werror -Wno-unused -std=c99 -pedantic -Wstrict-aliasing \
	-Wstrict-aliasing=2 -Wmissing-field-initializers -Wconversion -Wextra \
	-D_BSD_SOURCE -D_DEFAULT_SOURCE $(INCLUDE)

.PHONY: all pre-build dev dev-pre-build clean

all: pre-build $(EXE)
#	strip $(EXE)

pre-build:
# 	Make build directory.
	@if ! [ -d $(BUILD_DIR) ]; then \
		mkdir $(BUILD_DIR); \
	fi

dev: dev-pre-build $(EXE)

OBJECTS=\
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/common.o \
	$(BUILD_DIR)/pcode.o \
	$(BUILD_DIR)/load.o \
	$(BUILD_DIR)/discover.o \
	$(BUILD_DIR)/recover.o \
	$(BUILD_DIR)/codegen.o \
	$(BUILD_DIR)/builtin.o \
	$(BUILD_DIR)/analyze.o

# Compile executable.
$(EXE): $(OBJECTS)
	$(CC) -o $@ $^

# Compile: src/
$(BUILD_DIR)/main.o: \
	src/main.c \
	src/task.h \
	src/common.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/common.o: \
	src/common.c \
	src/common.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/pcode.o: \
	src/pcode.c \
	src/common.h \
	src/pcode.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/load.o: \
	src/load.c \
	src/task.h \
	src/common.h \
	src/pcode.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/discover.o: \
	src/discover.c \
	src/task.h \
	src/common.h \
	src/pcode.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/recover.o: \
	src/recover.c \
	src/task.h \
	src/common.h \
	src/pcode.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/codegen.o: \
	src/codegen.c \
	src/task.h \
	src/common.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/builtin.o: \
	src/builtin.c \
	src/task.h \
	src/common.h \
	src/pcode.h
	$(CC) -c $(OPTIONS) -o $@ $<

$(BUILD_DIR)/analyze.o: \
	src/analyze.c \
	src/task.h \
	src/common.h
	$(CC) -c $(OPTIONS) -o $@ $<

# Removes executable and build directory.
clean:
	@if [ -d $(BUILD_DIR) ]; then \
		for object in $(OBJECTS); do \
			if [ -f $$object ]; then \
				rm $$object; \
			fi; \
		done; \
		rmdir \
			$(BUILD_DIR);
	fi
	@if [ -f $(EXE) ]; then \
		rm $(EXE); \
	fi
