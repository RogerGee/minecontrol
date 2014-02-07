####################################################################################################
#  Makefile for project `minecraft-controller'                                                     # 
####################################################################################################
.PHONY: debug install clean

# programs and options
ifeq ($(MAKECMDGOALS),debug)
PROGRAM = minecraft-controller-debug
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = dobj/
else
PROGRAM = minecraft-controller
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = obj/
endif
LINK = g++
LIBRARY = -lrlibrary -pthread -lcrypt
OUT = -o 

# dependencies
PIPE_H = pipe.h
DOMAIN_SOCKET_H = domain-socket.h
MINECRAFT_SERVER_H = minecraft-server.h $(PIPE_H)
MINECRAFT_SERVER_PROPERTIES_H = minecraft-server-properties.h

# object code
OBJECTS = minecraft-controller.o minecraft-server.o minecraft-server-properties.o domain-socket.o pipe.o 

# make objects relative to object directory
OBJECTS := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS))

all: $(OBJECT_DIRECTORY) $(PROGRAM)
#make -C client

debug: $(OBJECT_DIRECTORY) $(PROGRAM)
#make -C client

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIBRARY)

$(OBJECT_DIRECTORY)minecraft-controller.o: minecraft-controller.cpp $(DOMAIN_SOCKET_H) $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-controller.o minecraft-controller.cpp

$(OBJECT_DIRECTORY)minecraft-server.o: minecraft-server.cpp $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server.o minecraft-server.cpp

$(OBJECT_DIRECTORY)minecraft-server-properties.o: minecraft-server-properties.cpp $(MINECRAFT_SERVER_PROPERTIES_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server-properties.o minecraft-server-properties.cpp

$(OBJECT_DIRECTORY)domain-socket.o: domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)domain-socket.o domain-socket.cpp

$(OBJECT_DIRECTORY)pipe.o: pipe.cpp $(PIPE_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)pipe.o pipe.cpp

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d obj ]; then rm -r --verbose obj; fi;
	if [ -d dobj ]; then rm -r --verbose dobj; fi;
	if [ -f minecraft-controller ]; then rm --verbose minecraft-controller; fi;
	if [ -f minecraft-controller-debug ]; then rm --verbose minecraft-controller-debug; fi;
