CC = g++
CFLAGS = -o2
SRC = context.cpp helper.cpp attr.cpp entry.cpp file.cpp recover.cpp
INC = context.hpp helper.hpp
OBJ = $(SRC:%.cpp=%.o)

.PHONY: all debug clean

all: ntfs.recover

ntfs.recover: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.cpp %.hpp $(INC)
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS = -ggdb3 -o0
debug: all

clean: 
	rm *.o ntfs.recover
