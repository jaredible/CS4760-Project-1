EXE	= mydu
EXE_OBJ	= mydu.o
OUTPUT	= $(EXE)

.SUFFIXES: .c .o

all: $(OUTPUT)

$(EXE): $(EXE_OBJ)
	gcc -Wall -g -o $@ $(EXE_OBJ)

.c.o:
	gcc -Wall -g -c $<

.PHONY: clean
clean:
	/bin/rm -f $(OUTPUT) *.o
