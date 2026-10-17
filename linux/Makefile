CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Os -s -fno-ident -fno-asynchronous-unwind-tables -Iheaders
SRCS = $(wildcard src/*.c)
TARGET = itsuki

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)
	@echo "[OK] Compilador Itsuki generado con exito: ./$(TARGET)"
	@./$(TARGET) --version

doctor: $(TARGET)
	@./$(TARGET) doctor

clean:
	rm -f $(TARGET) *.tmp.* *.o
