####################################################################################################
#  Makefile for project `minecraft-controller'; includes server and client programs                #
####################################################################################################
.PHONY: debug clean install uninstall

# programs and options
PROGRAM_NAME_SERVER = minecontrold
PROGRAM_NAME_SERVER_DEBUG = minecontrold-debug
PROGRAM_NAME_CLIENT = minecontrol
PROGRAM_NAME_CLIENT_DEBUG = minecontrol-debug
PACKAGE_NAME_SERVER = server.deb
PACKAGE_NAME_CLIENT = client.deb
OBJECT_DIRECTORY_NAME = obj/
OBJECT_DIRECTORY_NAME_DEBUG = dobj/
LINK = g++
LIBRARY_SERVER = -lrlibrary -pthread -lcrypt -lcrypto
LIBRARY_CLIENT = -lrlibrary -pthread -lcrypto -lncurses
OUT = -o 

# programs and options based on configuration
ifeq ($(MAKECMDGOALS),debug)
PROGRAM_SERVER = $(PROGRAM_NAME_SERVER_DEBUG)
PROGRAM_CLIENT = $(PROGRAM_NAME_CLIENT_DEBUG)
COMPILE = g++ -g -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
ifeq ($(MAKECMDGOALS),test)
COMPILE := $(COMPILE) -DMINECONTROL_TEST
endif
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME_DEBUG)
else
PROGRAM_SERVER = $(PROGRAM_NAME_SERVER)
PROGRAM_CLIENT = $(PROGRAM_NAME_CLIENT)
COMPILE = g++ -c -Wall -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable -pedantic-errors --std=gnu++0x
ifeq ($(MAKECMDGOALS),test)
COMPILE := $(COMPILE) -DMINECONTROL_TEST
endif
OBJECT_DIRECTORY = $(OBJECT_DIRECTORY_NAME)
endif

# dependencies
PIPE_H = pipe.h
SOCKET_H = socket.h
MUTEX_H = mutex.h
MINECONTROL_MISC_TYPES_H = minecontrol-misc-types.h
MINECRAFT_CONTROLLER_H = minecraft-controller.h
DOMAIN_SOCKET_H = domain-socket.h $(SOCKET_H)
NET_SOCKET_H = net-socket.h $(SOCKET_H)
MINECONTROL_PROTOCOL_H = minecontrol-protocol.h $(SOCKET_H)
MINECRAFT_SERVER_PROPERTIES_H = minecraft-server-properties.h minecraft-server-properties.tcc
MINECRAFT_SERVER_H = minecraft-server.h $(PIPE_H) $(MUTEX_H) $(MINECRAFT_SERVER_PROPERTIES_H) $(MINECONTROL_MISC_TYPES_H) $(MINECONTROL_AUTHORITY)
MINECONTROL_AUTHORITY_H = minecontrol-authority.h $(SOCKET_H) $(PIPE_H) $(MUTEX_H) $(MINECONTROL_MISC_TYPES_H)
MINECONTROL_CLIENT_H = minecontrol-client.h $(SOCKET_H) $(MUTEX_H) $(MINECONTROL_PROTOCOL_H) $(MINECONTROL_MISC_TYPES_H)

# object code
OBJECTS_SERVER = minecraft-controller.o minecraft-server.o minecraft-server-properties.o minecontrol-client.o minecontrol-authority.o socket.o domain-socket.o net-socket.o pipe.o mutex.o minecontrol-protocol.o
OBJECTS_CLIENT = minecontrol.o socket.o domain-socket.o net-socket.o mutex.o minecontrol-protocol.o

# make objects relative to object directory
OBJECTS_SERVER := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS_SERVER))
OBJECTS_CLIENT := $(addprefix $(OBJECT_DIRECTORY),$(OBJECTS_CLIENT))

all: $(OBJECT_DIRECTORY) $(PROGRAM_SERVER) $(PROGRAM_CLIENT)
debug: $(OBJECT_DIRECTORY) $(PROGRAM_SERVER) $(PROGRAM_CLIENT)
test: $(OBJECT_DIRECTORY) $(PROGRAM_SERVER) $(PROGRAM_CLIENT)

$(PROGRAM_SERVER): $(OBJECTS_SERVER)
	$(LINK) $(OUT)$(PROGRAM_SERVER) $(OBJECTS_SERVER) $(LIBRARY_SERVER)
	chmod go-rwx $(PROGRAM_SERVER)

$(PROGRAM_CLIENT): $(OBJECTS_CLIENT)
	$(LINK) $(OUT)$(PROGRAM_CLIENT) $(OBJECTS_CLIENT) $(LIBRARY_CLIENT)

$(OBJECT_DIRECTORY)minecontrol.o: minecontrol.cpp $(DOMAIN_SOCKET_H) $(NET_SOCKET_H) $(MUTEX_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol.o minecontrol.cpp

$(OBJECT_DIRECTORY)minecraft-controller.o: minecraft-controller.cpp $(MINECRAFT_SERVER_H) $(MINECONTROL_CLIENT_H) $(MINECRAFT_CONTROLLER_H) $(DOMAIN_SOCKET_H) $(NET_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-controller.o minecraft-controller.cpp

$(OBJECT_DIRECTORY)minecraft-server.o: minecraft-server.cpp $(MINECRAFT_SERVER_H) $(MINECRAFT_CONTROLLER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server.o minecraft-server.cpp

$(OBJECT_DIRECTORY)minecraft-server-properties.o: minecraft-server-properties.cpp $(MINECRAFT_SERVER_PROPERTIES_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecraft-server-properties.o minecraft-server-properties.cpp

$(OBJECT_DIRECTORY)minecontrol-client.o: minecontrol-client.cpp $(MINECONTROL_CLIENT_H) $(MINECRAFT_CONTROLLER_H) $(MINECRAFT_SERVER_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol-client.o minecontrol-client.cpp

$(OBJECT_DIRECTORY)minecontrol-authority.o: minecontrol-authority.cpp $(MINECONTROL_AUTHORITY_H) $(MINECONTROL_PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol-authority.o minecontrol-authority.cpp

$(OBJECT_DIRECTORY)minecontrol-protocol.o: minecontrol-protocol.cpp $(MINECONTROL_PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)minecontrol-protocol.o minecontrol-protocol.cpp

$(OBJECT_DIRECTORY)socket.o: socket.cpp $(SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)socket.o socket.cpp

$(OBJECT_DIRECTORY)domain-socket.o: domain-socket.cpp $(DOMAIN_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)domain-socket.o domain-socket.cpp

$(OBJECT_DIRECTORY)net-socket.o: net-socket.cpp $(NET_SOCKET_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)net-socket.o net-socket.cpp

$(OBJECT_DIRECTORY)pipe.o: pipe.cpp $(PIPE_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)pipe.o pipe.cpp

$(OBJECT_DIRECTORY)mutex.o: mutex.cpp $(MUTEX_H)
	$(COMPILE) $(OUT)$(OBJECT_DIRECTORY)mutex.o mutex.cpp

$(OBJECT_DIRECTORY):
	mkdir $(OBJECT_DIRECTORY)

clean:
	if [ -d $(OBJECT_DIRECTORY_NAME) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME); fi;
	if [ -d $(OBJECT_DIRECTORY_NAME_DEBUG) ]; then rm -r --verbose $(OBJECT_DIRECTORY_NAME_DEBUG); fi;
	if [ -f $(PROGRAM_NAME_SERVER) ]; then rm --verbose $(PROGRAM_NAME_SERVER); fi;
	if [ -f $(PROGRAM_NAME_SERVER_DEBUG) ]; then rm --verbose $(PROGRAM_NAME_SERVER_DEBUG); fi;
	if [ -f $(PROGRAM_NAME_CLIENT) ]; then rm --verbose $(PROGRAM_NAME_CLIENT); fi;
	if [ -f $(PROGRAM_NAME_CLIENT_DEBUG) ]; then rm --verbose $(PROGRAM_NAME_CLIENT_DEBUG); fi;
	if [ -f $(PACKAGE_NAME) ]; then rm --verbose $(PACKAGE_NAME); fi;

install:
	if [ -f $(PROGRAM_NAME_SERVER) ]; then cp $(PROGRAM_NAME_SERVER) /usr/local/bin; chmod go-rwx /usr/bin/$(PROGRAM_NAME_SERVER); fi;
	if [ -f $(PROGRAM_NAME_CLIENT) ]; then cp $(PROGRAM_NAME_CLIENT) /usr/local/bin; fi;

package: $(PACKAGE_NAME_SERVER) $(PACKAGE_NAME_CLIENT)

$(PACKAGE_NAME_SERVER):
	mkdir -p dpkg-server/usr/bin
	if [ -f $(PROGRAM_NAME_SERVER) ]; then cp $(PROGRAM_NAME_SERVER) dpkg-server/usr/bin; chmod go-rwx dpkg-server/usr/bin/$(PROGRAM_NAME_SERVER); fi;
	dpkg-deb --build dpkg-server/ $(PACKAGE_NAME_SERVER)

$(PACKAGE_NAME_CLIENT):
	mkdir -p dpkg-client/usr/bin
	if [ -f $(PROGRAM_NAME_CLIENT) ]; then cp $(PROGRAM_NAME_CLIENT) dpkg-client/usr/bin; fi;
	dpkg-deb --build dpkg-client/ $(PACKAGE_NAME_CLIENT)

uninstall:
	if [ -f /usr/local/bin/$(PROGRAM_NAME_SERVER) ]; then rm --verbose /usr/local/bin/$(PROGRAM_NAME_SERVER); fi;
	if [ -f /usr/local/bin/$(PROGRAM_NAME_CLIENT) ]; then rm --verbose /usr/local/bin/$(PROGRAM_NAME_CLIENT); fi;
