SRCDIR	= src
OBJDIR	= obj

SRC_FILES	= $(wildcard $(SRCDIR)/*.cpp)
OBJ_FILES	= $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC_FILES))

HEADER	= gaomon-s620.hpp
OUT	= driver
CC	= g++
FLAGS	= -g -c -Wall
LFLAGS	= -lusb-1.0 -lrt

all: $(OBJDIR) $(OBJ_FILES)
	$(CC) -g $(OBJ_FILES) -o $(OUT) $(LFLAGS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(FLAGS) -c $< -o $@ 

clean:
	rm -rf $(OBJDIR)

clean-all: clean
	rm  $(OUT)
