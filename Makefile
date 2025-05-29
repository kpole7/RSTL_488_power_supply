CC	        = gcc
CXX	        = g++
OBJCOPY	    = objcopy

CFLAGS	    =  -g -Wall -I. -IfreeModbus/include -IfreeModbus/tcp -IfreeModbus/port
CFLAGS      += -pthread -MMD -MP

CCFLAGS	    =  -g -Wall -Wextra -I. -MMD -MP

LDFLAGS     =  -lfltk -lX11 -lpthread

CSRC        = modbusTcpSlave.c \
              freeModbus/port/portother.c \
              freeModbus/port/portevent.c \
              freeModbus/port/porttcp.c \
              freeModbus/mb.c \
              freeModbus/tcp/mbtcp.c \
              freeModbus/functions/mbfunccoils.c \
              freeModbus/functions/mbfuncdiag.c \
              freeModbus/functions/mbfuncholding.c \
              freeModbus/functions/mbfuncinput.c \
              freeModbus/functions/mbfuncother.c \
              freeModbus/functions/mbfuncdisc.c \
              freeModbus/functions/mbutils.c 

CCSRC       = powerSourceRSTL.cpp \
              rstlProtocolMaster.cpp \
              multiChannel.cpp \
              dataSharingInterface.cpp \
              graphicalUserInterface.cpp \
              modbusTcpMaster.cpp \
              git_revision.cpp

OBJS        = $(CCSRC:.cpp=.o) $(CSRC:.c=.o)
DEPS        = $(CCSRC:.cpp=.d) $(CSRC:.c=.d)

BIN         = powerSourceRSTL

.PHONY: clean all

all: git_revision.cpp $(BIN)

git_revision.cpp:
	echo "// File generated automatically by make\nconst char TcpSlaveIdentifier[40] = \"ID: git commit time $$(git log -1 --format='%cd' --date=format:'%Y-%m-%d %H:%M:%S')\";" > git_revision.cpp

$(BIN): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)
	cp $(BIN) testing_TCP_master
	cp $(BIN) testing_TCP_slave

clean:
	rm -f $(DEPS)
	rm -f $(OBJS)
	rm -f $(BIN) testing_TCP_master/$(BIN)
	rm -f $(BIN) testing_TCP_slave/$(BIN)
#	rm -f git_revision.cpp

# ---------------------------------------------------------------------------
# rules for code generation
# ---------------------------------------------------------------------------
%.o:    %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:    %.cpp
	$(CXX) $(CCFLAGS) -o $@ -c $<

# ---------------------------------------------------------------------------
#  # compiler generated dependencies
# ---------------------------------------------------------------------------
-include $(DEPS)

