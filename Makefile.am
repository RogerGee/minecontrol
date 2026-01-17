# Makefile.am - minecontrol

bin_PROGRAMS = minecontrol
sbin_PROGRAMS = minecontrold

minecontrold_SOURCES = domain-socket.cpp minecontrol-authority.cpp minecontrol-client.cpp \
	minecontrol-protocol.cpp minecraft-controller.cpp minecraft-server.cpp \
	minecraft-server-properties.cpp mutex.cpp net-socket.cpp pipe.cpp socket.cpp

minecontrol_SOURCES = minecontrol.cpp minecontrol-protocol.cpp mutex.cpp net-socket.cpp \
	domain-socket.cpp socket.cpp

minecontrol_LDADD = -lrlibrary -lssl -lcrypto -lncurses -lreadline
minecontrold_LDADD = -lrlibrary -lssl -lcrypto -lcrypt
