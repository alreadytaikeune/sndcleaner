CC=g++
LIBPATH=-L./lib/libsvm/
LIBS_FFMPEG=-lavutil -lavformat -lavcodec -lswscale -lswresample

LIBS_MISC= -lstdc++ -lm -lz -lpthread -lplplotd -lSDL2 \
-lfftw3 -lboost_program_options -lboost_system -lboost_filesystem

LIBS_LEARN= -l:libsvm.so.2
LIBS=$(LIBS_MISC) $(LIBS_FFMPEG) $(LIBS_LEARN)
INC=-Ilib
CFLAGS=-std=c++11 -g -O0


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
	@echo $(LINKER) $@ $(LFLAGS) $(OBJ) $(LIBPATH) $(LIBS)
	@$(LINKER) $@ $(LFLAGS) $(OBJ) $(LIBPATH) $(LIBS)
	@echo "Linking complete!"

$(OBJ): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	@$(CC) $(CFLAGS) -c $< -o $@ $(INC)
	@echo "Compiled "$<" successfully!"

build:
	@- if [ ! -d "$(OBJDIR)" ]; then mkdir "$(OBJDIR)"; fi
	@- if [ ! -d "$(BINDIR)" ]; then mkdir "$(BINDIR)"; fi


.PHONY: lib

lib:
	cd lib/libsvm && make lib


.PHONY: clean
clean:
	@- $(RM) $(TARGET)
	@- $(RM) $(OBJ)