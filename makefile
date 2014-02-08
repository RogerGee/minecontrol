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
MUTEX_H = mutex.h
MINECRAFT_CONTROLLER_H = minecraft-controller.h
MINECRAFT_SERVER_H = minecraft-server.h $(PIPE_H)
MINECRAFT_SERVER_PROPERTIES_H = minecraft-server-properties.h minecraft-server-properties.tcc
CLIENT_H = client.h $(DOMAIN_SOCKET_H) $(MUTEX_H) $(MINECRAFT_SERVER_H)

# object code
OBJECTS = minecraft-controller.o minecraft-server.o minecraft-server-properties.o client.o domain-socket.o pipe.o mutex.o

# make objects relative to object directory
OBJECTS := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS))

all: $(OBJECT_DIRECTORY) $(PROGRAM)
#make -C client

debug: $(OBJECT_DIRECTORY) $(PROGRAM)
#make -C client

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIBRARY)

$(OBJECT_DIRECTORY)minecraft-controller.o: minecraft-controller.cpp $(MINECRAFT_SERVER_H) $(CLIENT_H) $(MINECRAFT_CONTROLLER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-controller.o minecraft-controller.cpp

$(OBJECT_DIRECTORY)minecraft-server.o: minecraft-server.cpp $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server.o minecraft-server.cpp

$(OBJECT_DIRECTORY)minecraft-server-properties.o: minecraft-server-properties.cpp $(MINECRAFT_SERVER_PROPERTIES_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server-properties.o minecraft-server-properties.cpp

$(OBJECT_DIRECTORY)client.o: client.cpp $(CLIENT_H) $(MINECRAFT_CONTROLLER_H) $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)client.o client.cpp

$(OBJECT_DIRECTORY)domain-socket.o: domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)domain-socket.o domain-socket.cpp

$(OBJECT_DIRECTORY)pipe.o: pipe.cpp $(PIPE_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)pipe.o pipe.cpp

$(OBJECT_DIRECTORY)mutex.o: mutex.cpp $(MUTEX_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)mutex.o mutex.cpp

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d obj ]; then rm -r --verbose obj; fi;
	if [ -d dobj ]; then rm -r --verbose dobj; fi;
	if [ -f minecraft-controller ]; then rm --verbose minecraft-controller; fi;
	if [ -f minecraft-controller-debug ]; then rm --verbose minecraft-controller-debug; fi;
