CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lrt -lpthread
TARGET = pipe_experiment

all: $(TARGET)

$(TARGET): pipe_experiment.c
	$(CC) $(CFLAGS) -o $(TARGET) pipe_eximent.c $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

