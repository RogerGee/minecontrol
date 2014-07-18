###############################################################################
# Makefile for standard minecontrol authority programs and libraries ##########
###############################################################################
.PHONY: debug clean install uninstall

# constants
OBJDIR_NAME = obj/
OBJDIR_NAME_DEBUG = dobj/
AUTH_LIB_VERSION_MAJOR = 1
AUTH_LIB_VERSION_MINOR = 0.0
AUTH_LIB_REALNAME = libminecontrol-api.so.$(AUTH_LIB_VERSION_MAJOR).$(AUTH_LIB_VERSION_MINOR)
AUTH_LIB_SONAME = libminecontrol-api.so.$(AUTH_LIB_VERSION_MAJOR)
AUTH_LIB_LINKNAME = libmineconrol-api.so
ifeq ($(MAKECMDGOALS),debug)
OBJDIR = $(OBJDIR_NAME_DEBUG)
COMPILE = gcc -ansi -c -g -o 
COMPILE_SHARED = gcc -ansi -c -g -o 
LINK = gcc -g -pthread -o
else
OBJDIR = $(OBJDIR_NAME)
COMPILE = gcc -ansi -c -o 
COMPILE_SHARED = gcc -ansi -fPIC -c -o 
LINK = gcc -pthread -o
endif

# object files
OBJECTS_JOURNAL = journal.o
OBJECTS_SPAWNER = spawner.o
OBJECTS_CONSTRUCT = construct.o
OBJECTS_LIB = minecontrol-api.o tree.o

# make object files relative to object directory
OBJECTS_JOURNAL := $(addprefix $(OBJDIR),$(OBJECTS_JOURNAL))
OBJECTS_SPAWNER := $(addprefix $(OBJDIR),$(OBJECTS_SPAWNER))
OBJECTS_CONSTRUCT := $(addprefix $(OBJDIR),$(OBJECTS_CONSTRUCT))
OBJECTS_LIB := $(addprefix $(OBJDIR),$(OBJECTS_LIB))

# recipes:

all: $(OBJDIR) $(AUTH_LIB_REALNAME) journal
debug: $(OBJDIR) $(OBJECTS_LIB) journal-debug

# rules for shared library
$(AUTH_LIB_REALNAME): $(OBJECTS_LIB)
	gcc -shared -Wl,-soname,$(AUTH_LIB_SONAME) -o $(AUTH_LIB_REALNAME) $(OBJECTS_LIB)
$(OBJDIR)minecontrol-api.o: minecontrol-api.c minecontrol-api.h
	$(COMPILE_SHARED)$@ minecontrol-api.c
$(OBJDIR)tree.o: tree.c tree.h
	$(COMPILE_SHARED)$@ tree.c

# rules for 'journal' program
journal: $(OBJECTS_JOURNAL) $(AUTH_LIB_REALNAME)
	$(LINK) journal $(OBJECTS_JOURNAL) $(AUTH_LIB_REALNAME)
journal-debug: $(OBJECTS_JOURNAL) $(OBJECTS_LIB)
	$(LINK) journal-debug $(OBJECTS_JOURNAL) $(OBJECTS_LIB)
$(OBJDIR)journal.o: journal.c tree.h minecontrol-api.h
	$(COMPILE)$@ journal.c

# rule to make object directory
$(OBJDIR):
	mkdir $(OBJDIR)

# clean rule
clean:
	if [ -d $(OBJDIR_NAME) ]; then rm -r --verbose $(OBJDIR_NAME); fi
	if [ -d $(OBJDIR_NAME_DEBUG) ]; then rm -r --verbose $(OBJDIR_NAME_DEBUG); fi
	if [ -f $(AUTH_LIB_REALNAME) ]; then rm --verbose $(AUTH_LIB_REALNAME); fi
	if [ -f journal ]; then rm --verbose journal; fi
	if [ -f journal-debug ]; then rm --verbose journal-debug; fi
