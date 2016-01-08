CC=g++
LIBS_FFMPEG=-lavutil -lavformat -lavcodec -lswscale -lswresample
LIBS_MISC= -lstdc++ -lm -lz -lpthread -lplplotd
LIBS=$(LIBS_MISC) $(LIBS_FFMPEG)
CFLAGS=-std=c++11 -g


LINKER   = gcc -o
# linking flags here
LFLAGS   = -Wall 

SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

TARGET=sndcleaner
SRC=$(wildcard src/**/*.cpp src/*.cpp)
OBJ=$(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

$(BINDIR)/$(TARGET): $(OBJ)
	@echo $(LINKER) $@ $(LFLAGS) $(OBJ) $(LIBS)
	@$(LINKER) $@ $(LFLAGS) $(OBJ) $(LIBS)
	@echo "Linking complete!"

$(OBJ): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"


build:
	@- if [ ! -d "$(OBJDIR)" ]; then mkdir "$(OBJDIR)"; fi
	@- if [ ! -d "$(BINDIR)" ]; then mkdir "$(BINDIR)"; fi


.PHONY: clean
clean:
	@- $(RM) $(TARGET)
	@- $(RM) $(OBJ)