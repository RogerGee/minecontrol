################################################################################
# Makefile for project `minecraft-controller/minecraft' (client program)       #
################################################################################
.PHONY: debug clean install uninstall

# programs and options
PROGRAM_NAME = minecontrol
PROGRAM_NAME_DEBUG = minecontrol-debug
OBJECT_DIRECTORY_NAME = obj/
OBJECT_DIRECTORY_NAME_DEBUG = dobj/
ifeq ($(MAKECMDGOALS),debug)
PROGRAM = $(PROGRAM_NAME_DEBUG)
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME_DEBUG)
PREV_OBJ_DIRECTORY = ../dobj/
else
PROGRAM = $(PROGRAM_NAME)
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME)
PREV_OBJECT_DIRECTORY = ../obj/
endif
LINK = g++
INCLUDE = -I..
LIBRARY = -lrlibrary
OUT = -o 

# dependencies
DOMAIN_SOCKET_H = ../domain-socket.h

# object code
OBJECTS = minecontrol.o
LIB_OBJECTS = domain-socket.o

# make objects relative to object directories
OBJECTS := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS))
LIB_OBJECTS := $(addprefix $(PREV_OBJECT_DIRECTORY),$(LIB_OBJECTS))

all: $(OBJECT_DIRECTORY) $(PROGRAM)

debug: $(OBJECT_DIRECTORY) $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIB_OBJECTS) $(LIBRARY)

$(OBJECT_DIRECTORY)minecontrol.o: minecontrol.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol.o minecontrol.cpp $(INCLUDE)

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d $(OBJECT_DIRECTORY_NAME) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME); fi;
	if [ -d $(OBJECT_DIRECTORY_NAME_DEBUG) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME_DEBUG); fi;
	if [ -f $(PROGRAM_NAME) ]; then rm --verbose $(PROGRAM_NAME); fi;
	if [ -f $(PROGRAM_NAME_DEBUG) ]; then rm --verbose $(PROGRAM_NAME_DEBUG); fi;

install:
	if [ -f $(PROGRAM_NAME) ]; then mv $(PROGRAM_NAME) /usr/local/bin; fi;

uninstall:
	if [ -f /usr/local/bin/$(PROGRAM_NAME) ]; then rm --verbose /usr/local/bin/$(PROGRAM_NAME); fi;

