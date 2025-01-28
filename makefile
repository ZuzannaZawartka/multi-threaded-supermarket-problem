# Kompilator C
CC = gcc

# Flagi kompilatora
CFLAGS = -Wall -g -I./include -pthread

LDFLAGS = -pthread

# Pliki źródłowe dla supermarket
SRC = src/main.c src/creator_customer.c src/cashier.c src/firefighter.c src/shared_memory.c src/manager_cashiers.c

# Pliki obiektowe (dla każdego .c tworzymy .o)
OBJ = $(SRC:.c=.o)

# Nagłówki
HEADERS = include/main.h include/creator_customer.h include/cashier.h include/firefighter.h include/shared_memory.h include/manager_cashiers.h

# Nazwa pliku wynikowego dla głównego programu
OUT = supermarket

# Pliki źródłowe dla klienta
CUSTOMER_SRC = src/customer.c src/shared_memory.c src/creator_customer.c src/cashier.c src/manager_cashiers.c
CUSTOMER_OBJ = $(CUSTOMER_SRC:.c=.o)

# Nazwa pliku wynikowego dla programu klienta
CUSTOMER_OUT = src/customer

# Reguła domyślna - kompilacja programu supermarket i customer
all: $(OUT) $(CUSTOMER_OUT)

# Reguła kompilacji programu supermarket
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(OUT)

# Reguła kompilacji programu customer
$(CUSTOMER_OUT): $(CUSTOMER_OBJ)
	$(CC) $(CFLAGS) $(CUSTOMER_OBJ) -o $(CUSTOMER_OUT)

# Reguła kompilacji plików obiektowych
src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Wykonanie programu supermarket
run: $(OUT)
	./$(OUT)

# Wykonanie programu customer
run_customer: $(CUSTOMER_OUT)
	./$(CUSTOMER_OUT)

# Reguła czyszczenia (usuwanie plików wynikowych)
clean:
	rm -f $(OUT) $(OBJ) $(CUSTOMER_OUT) $(CUSTOMER_OBJ)

# Wykonanie pełnego czyszczenia
fclean: clean
	rm -f *.o
