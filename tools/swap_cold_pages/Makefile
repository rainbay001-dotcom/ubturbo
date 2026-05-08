CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=gnu11
TARGET  = swap_cold_pages

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: clean
