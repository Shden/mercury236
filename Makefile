CFLAGS = -g -std=c99
LIBS = -lm
OBJS = main.o 


mercury236: $(OBJS)
	$(CC) -o mercury236 $(CFLAGS) $(OBJS) -lm

clean:
	rm *.o mercury236
