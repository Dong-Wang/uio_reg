TARGET=uio_reg
OBJLIST=uio_reg.o

CC=gcc
CFLAG=-g -Wall

all:$(TARGET)

$(TARGET): $(OBJLIST)
	$(CC) $(CFLAG) $^ -o $@

%.o:%.c
	$(CC) $(CFLAG) -c $^

clean:
	-rm -f *.o
	-rm -f $(TARGET)
