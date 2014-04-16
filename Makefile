CFLAGS = -g -std=c99
LIBS = -lm
OBJS = mercury236.o 


mercury236: $(OBJS)
	$(CC) -o mercury236 $(CFLAGS) $(OBJS) -lm

clean:
	rm *.o mercury236
