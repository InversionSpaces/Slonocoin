CC = g++
CFLAGS = -std=c++17 -w -Ofast
SRCS = slonocoin.cpp BlockFactory.cpp 
OBJS = $(SRCS: .cpp = .o)
PROG = slonocoin
INC = boost_1_70_0/
LIB = -L/usr/local/lib -lpaho-mqttpp3 -lpaho-mqtt3as -lcrypto -lpthread 

INC_PARAMS=$(INC:%=-I%)

all: $(OBJS)
	time $(CC) $(CFLAGS) $(INC_PARAMS) $(SRCS) $(LIB) -o $(PROG) 

run: all
	./$(PROG)

clean:
	rm $(OBJS)
