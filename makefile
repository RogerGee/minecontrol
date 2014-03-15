####################################################################################################
#  Makefile for project `minecraft-controller'                                                     #
####################################################################################################
.PHONY: debug clean install uninstall

# programs and options
PROGRAM_NAME = minecontrold
PROGRAM_NAME_DEBUG = minecontrold-debug
OBJECT_DIRECTORY_NAME = obj/
OBJECT_DIRECTORY_NAME_DEBUG = dobj/
ifeq ($(MAKECMDGOALS),debug)
PROGRAM = $(PROGRAM_DEBUG)
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME_DEBUG)
else
PROGRAM = $(PROGRAM_NAME)
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME)
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
MINECONTROL_CLIENT_H = minecontrol-client.h $(DOMAIN_SOCKET_H) $(MUTEX_H) $(MINECRAFT_SERVER_H)
MINECONTROL_AUTHORITY_H = minecontrol-authority.h $(PIPE_H)

# object code
OBJECTS = minecraft-controller.o minecraft-server.o minecraft-server-properties.o minecontrol-client.o minecontrol-authority.o domain-socket.o pipe.o mutex.o

# make objects relative to object directory
OBJECTS := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS))

all: $(OBJECT_DIRECTORY) $(PROGRAM)
	make -C client

debug: $(OBJECT_DIRECTORY) $(PROGRAM)
	make -C client debug

$(PROGRAM): $(OBJECTS)
	$(LINK) $(OUT)$(PROGRAM) $(OBJECTS) $(LIBRARY)

$(OBJECT_DIRECTORY)minecraft-controller.o: minecraft-controller.cpp $(MINECRAFT_SERVER_H) $(MINECONTROL_CLIENT_H) $(MINECRAFT_CONTROLLER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-controller.o minecraft-controller.cpp

$(OBJECT_DIRECTORY)minecraft-server.o: minecraft-server.cpp $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server.o minecraft-server.cpp

$(OBJECT_DIRECTORY)minecraft-server-properties.o: minecraft-server-properties.cpp $(MINECRAFT_SERVER_PROPERTIES_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server-properties.o minecraft-server-properties.cpp

$(OBJECT_DIRECTORY)minecontrol-client.o: minecontrol-client.cpp $(MINECONTROL_CLIENT_H) $(MINECRAFT_CONTROLLER_H) $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol-client.o minecontrol-client.cpp

$(OBJECT_DIRECTORY)minecontrol-authority.o: minecontrol-authority.cpp $(MINECONTROL_AUTHORITY_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol-authority.o minecontrol-authority.cpp

$(OBJECT_DIRECTORY)domain-socket.o: domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)domain-socket.o domain-socket.cpp

$(OBJECT_DIRECTORY)pipe.o: pipe.cpp $(PIPE_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)pipe.o pipe.cpp

$(OBJECT_DIRECTORY)mutex.o: mutex.cpp $(MUTEX_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)mutex.o mutex.cpp

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d $(OBJECT_DIRECTORY_NAME) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME); fi;
	if [ -d $(OBJECT_DIRECTORY_NAME_DEBUG) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME_DEBUG); fi;
	if [ -f $(PROGRAM_NAME) ]; then rm --verbose $(PROGRAM_NAME); fi;
	if [ -f $(PROGRAM_NAME_DEBUG) ]; then rm --verbose $(PROGRAM_NAME_DEBUG); fi;
	make -C client clean

install:
	if [ -f $(PROGRAM_NAME) ]; then mv $(PROGRAM_NAME) /usr/local/bin; chmod go-rwx /usr/local/bin/$(PROGRAM_NAME); fi;
	make -C client install

uninstall:
	if [ -f /usr/local/bin/$(PROGRAM_NAME) ]; then rm --verbose /usr/local/bin/$(PROGRAM_NAME); fi;
	make -C client uninstall

