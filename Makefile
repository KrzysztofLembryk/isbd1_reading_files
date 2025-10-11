
CC := gcc
CFLAGS := -Wall -Wextra -O2 -I./libcrc/include
LDFLAGS := -L./libcrc/lib
LDLIBS := -lcrc

all: libcrc/lib/libcrc.a reading_files

libcrc/lib/libcrc.a:
    $(MAKE) -C libcrc

reading_files: 1_lab_task_reading_files.c
    $(CC) $(CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

clean:
    $(RM) reading_files
    $(MAKE) -C libcrc clean