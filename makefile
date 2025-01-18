# Kompilator C
CC = gcc

# Flagi kompilatora
CFLAGS = -Wall -g -I./include -pthread 

LDFLAGS = -pthread

# Pliki źródłowe (wszystkie pliki C w katalogu src)
SRC = src/main.c src/customer.c src/cashier.c src/firefighter.c src/process_manager.c src/shared_memory.c src/manager_cashiers.c

# Pliki obiektowe (dla każdego .c tworzymy .o)
OBJ = $(SRC:.c=.o)

# Nagłówki
HEADERS = include/main.h include/customer.h include/cashier.h include/firefighter.h include/process_manager.h include/shared_memory.h include/manager_cashiers.h

# Nazwa pliku wynikowego
OUT = supermarket

# Reguła domyślna - kompilacja programu
all: $(OUT)

# Reguła kompilacji programu
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(OUT)

# Reguła kompilacji plików obiektowych
# .c -> .o (np. main.c -> main.o)
src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Wykonanie programu
run: $(OUT)
	./$(OUT)

# Reguła czyszczenia (usuwanie plików wynikowych)
clean:
	rm -f $(OUT) $(OBJ)

# Wykonanie czyszczenia
fclean: clean
	rm -f *.o
