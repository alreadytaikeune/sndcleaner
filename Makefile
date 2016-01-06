CC=g++
LIBS_FFMPEG=-lavutil -lavformat -lavcodec -lswscale -lswresample
LIBS=-lpthread -lz -lm -lplplotd
FLAGS=-std=c++11 
DEPS=sndcleaner.h
OBJ=sndcleaner.o
%.o: %.cpp 
	$(CC) -c -o $@ $< $(LIBS_FFMPEG) $(LIBS) $(FLAGS)

sndcleaner: $(OBJ) 
	$(CC) -o $@ $^ $(LIBS_FFMPEG) $(LIBS) $(FLAGS)

clean:
	@- $(RM) $(NAME)
	@- $(RM) $(OBJ)