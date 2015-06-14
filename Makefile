PROG            = dyio
LIB             = dyio.a
CFLAGS		= -O -Wall -Werror
LDFLAGS		=
#CC		= i586-mingw32msvc-gcc
OBJS            = serial.o connect.o calls.o print.o

all:		$(LIB) $(PROG)

$(LIB):         $(OBJS)
		@rm -f $@
		$(AR) cq $@ $(OBJS)

$(PROG):        tool.o $(LIB)
		$(CC) $(LDFLAGS) tool.o $(LIB) -o $@

clean:
		rm -f $(PROG) *.o *.a *~ a.out
