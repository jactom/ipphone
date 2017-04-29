all:
	gcc -o cc -g `pkg-config --cflags ortp` chat_client.c `pkg-config --libs ortp` -lpulse -lpulse-simple -lrt -lm
	gcc -o cs -g `pkg-config --cflags ortp` chat_server.c `pkg-config --libs ortp` -lpulse -lpulse-simple -lrt -lm

clean:
	rm -f cc cs
