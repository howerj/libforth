CC=gcc
CFLAGS=-Wall -Wextra -g -pedantic -fPIC -std=c99
TARGET=forth
.PHONY: all clean
all: $(TARGET) lib$(TARGET).so
doc: $(TARGET).htm 
lib$(TARGET).a: lib$(TARGET).o
	ar rcs $@ $<
lib$(TARGET).so: lib$(TARGET).o lib$(TARGET).h
	$(CC) -shared -fPIC $< -o $@
lib$(TARGET).o: lib$(TARGET).c lib$(TARGET).h
	$(CC) $(CFLAGS) $< -c -o $@
unit: unit.c libforth.a
	$(CC) $(CFLAGS) $^ -o $@
$(TARGET): main.c lib$(TARGET).a
	$(CC) $(CFLAGS) $^ -o $@
$(TARGET).htm: $(TARGET).md
	markdown $^ > $@
run: $(TARGET)
	./$^
test: unit
	./$^
clean:
	rm -rf $(TARGET) unit *.a *.so *.o *.log *.htm doxygen
