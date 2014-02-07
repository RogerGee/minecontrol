################################################################################
# Makefile for project `minecraft-controller/minecraft' (client program)       #
################################################################################
.PHONY: debug clean install uninstall

# programs and options
ifeq ($(MAKECMDGOALS),debug)
PROGRAM = minecraft-debug
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = dobj/
else
PROGRAM = minecraft
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = obj/
endif
LINK = g++
INCLUDE = -I..
LIBRARY = -lrlibrary
OUT = -o 

# dependencies
DOMAIN_SOCKET_H = ../domain-socket.h

# object code
OBJECTS = minecraft.o domain-socket.o

# make objects relative to object directory
OBJECTS := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS))

all: $(OBJECT_DIRECTORY) $(PROGRAM)

debug: $(OBJECT_DIRECTORY) $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIBRARY)

$(OBJECT_DIRECTORY)minecraft.o: minecraft.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft.o minecraft.cpp $(INCLUDE)

$(OBJECT_DIRECTORY)domain-socket.o: ../domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)domain-socket.o ../domain-socket.cpp $(INCLUDE)

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d obj ]; then rm -r --verbose obj; fi;
	if [ -d dobj ]; then rm -r --verbose dobj; fi;
	if [ -f minecraft ]; then rm --verbose minecraft; fi;
	if [ -f minecraft-debug ]; then rm --verbose minecraft-debug; fi;
