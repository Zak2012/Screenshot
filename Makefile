## Compiler
CCXX = g++
CC = gcc

## Delete Commmand
DC = del

## Standard
STDXX = c++2a
STD = c17

## Executables Name
EXE = $(notdir $(CURDIR))

## Project Directories
INCDIR = include
LIBDIR = lib
OBJDIR = obj
SRCDIR = src

## Define Source
SOURCE = Application.cpp fpng.cpp
LIBS = gdi32 uuid ole32

OBJECT = $(addsuffix .o, $(SOURCE))

## Define File
SRC = $(addprefix $(SRCDIR)/, $(SOURCE))
INC = $(addprefix -I, $(INCDIR))
OBJ = $(addprefix $(OBJDIR)/, $(OBJECT))
LIB = $(addprefix -l, $(LIBS))

## Define Flags
CFLAGSXX = -c -g3 -Wall -std=$(STDXX) $(INC)  -msse4.1 -mpclmul
CFLAGS = -c -g3 -Wall -std=$(STD) $(INC)
LFLAGS = -L$(LIBDIR) $(LIB) -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive -static-libgcc -static-libstdc++  -msse4.1 -mpclmul -mwindows

## Define Scope
all : $(SRC) $(EXE)

## Compile C++ Files
$(OBJDIR)/%.cpp.o : $(SRCDIR)/%.cpp
	$(CCXX) $< $(CFLAGSXX) -o $@

## Compile C Files
$(OBJDIR)/%.c.o : $(SRCDIR)/%.c
	$(CC) $< $(CFLAGS) -o $@

## Link Object Files
$(EXE) : $(OBJ)
	$(CCXX) $^ $(LFLAGS) -o $@

## Clean Object Files
clean : $(subst /,\,$(OBJ))
	-$(DC) $^