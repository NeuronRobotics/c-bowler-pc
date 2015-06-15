#
# To build the library and dyio utility, use:
#   $ make
#

CC              = gcc
GITVERS         = $(shell git rev-list HEAD --count)
CFLAGS          = -O -Wall -Werror -DGITVERSION='"$(GITVERS)"'
LDFLAGS         =
PROG            = dyio
OBJS            = serial.o connect.o calls.o print.o
LIB             = libdyio.a

all:            $(LIB) $(PROG)

$(LIB):         $(OBJS)
		@rm -f $@
		$(AR) cq $@ $(OBJS)

$(PROG):        tool.o $(LIB)
		$(CC) $(LDFLAGS) tool.o -L. -ldyio -o $@

clean:
		rm -f $(PROG) *.o *.a *~ *.exe

###
calls.o: calls.c dyio.h
connect.o: connect.c dyio.h
print.o: print.c dyio.h
serial.o: serial.c dyio.h
tool.o: tool.c dyio.h
