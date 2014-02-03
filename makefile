####################################################################################################
#  Makefile for project `minecraft-controller'                                                     # 
####################################################################################################
.PHONY: debug install clean

# programs and options
ifeq ($(MAKECMDGOALS),debug)
PROGRAM = minecraft-controller-debug
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_FOLDER = dobj/
else
PROGRAM = minecraft-controller-debug
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_FOLDER = obj/
endif
LINK = g++
LIBRARY = -lrlibrary
OUT = -o 

# dependencies
DOMAIN_SOCKET_H = domain-socket.h

# object code
OBJECTS = minecraft-controller.o domain-socket.o

# make objects relative to object directory
OBJECTS := $(addprefix $(OBJECT_FOLDER),$(OBJECTS))

all: $(OBJECT_FOLDER) $(PROGRAM)
#make -C client

debug: $(OBJECT_FOLDER) $(PROGRAM)
#make -C client

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIBRARY)

$(OBJECT_FOLDER)minecraft-controller.o: minecraft-controller.cpp
	$(COMPILE) $(OUT)$(OBJECT_FOLDER)minecraft-controller.o minecraft-controller.cpp

$(OBJECT_FOLDER)domain-socket.o: domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_FOLDER)domain-socket.o domain-socket.cpp

$(OBJECT_FOLDER):
	mkdir $(OBJECT_FOLDER)

clean:
	if [ -d obj ]; then rm -r --verbose obj; fi;
	if [ -d dobj ]; then rm -r --verbose dobj; fi;
	if [ -f minecraft-controller ]; then rm --verbose minecraft-controller; fi;
	if [ -f minecraft-controller-debug ]; then rm --verbose minecraft-controller-debug; fi;
