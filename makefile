CC = g++
CFLAGS = -o2
SRC = context.cpp helper.cpp attr.cpp entry.cpp file.cpp recover.cpp
INC = context.hpp helper.hpp
OBJ = $(SRC:%.cpp=%.o)

.PHONY: all debug clean

all: ntfs.recover

debug: CFLAGS = -ggdb3 -o0
debug: all

ntfs.recover: $(OBJ)
	$(CC) $^ -o $@

%.o: %.cpp %.hpp $(INC)
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm *.o ntfs.recover
