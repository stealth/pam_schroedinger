#

CC=cc
CXX=c++
CXXFLAGS=-c -Wall -O2 -ansi -pedantic -std=c++11 $(DEFS)
CXXFLAGS+=-DHAVE_UNIX98
CFLAGS=-c -Wall -O2 -ansi -pedantic -std=c99 $(DEFS)

all: enabler pam_schroedinger

clean:
	rm -f *.o

pam_schroedinger: pam_schroedinger.c
	$(CC) -fPIC $(CFLAGS) pam_schroedinger.c
	$(CC) -shared -Wl,-soname,pam_schroedinger.so -o pam_schroedinger.so pam_schroedinger.o

enabler: pty.o pty98.o enabler.o
	$(CXX) enabler.o pty.o pty98.o -o enabler

enabler.o: enabler.cc
	$(CXX) $(CXXFLAGS) enabler.cc

pty.o: pty.cc
	$(CXX) $(CXXFLAGS) pty.cc

pty98.o: pty98.cc
	$(CXX) $(CXXFLAGS) pty98.cc


