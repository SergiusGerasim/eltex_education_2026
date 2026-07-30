# Задание:
Доработать решение задачи 4.1 (список контактов) так, чтобы структуры и
функции по работе с двухсвязным упорядоченным списком находились в статической библиотеке.

## Коментарии по заданию:
Изменения внесены только в Makefile (разделение на .h и .c файлы было реализованно изначально).
- Прописана команда комплиляции исходника phone_book в объектный файл phone_book.o
- далее из объектного файла создаётся архив `ar rcs build/libphonebook.a build/phone_book.o`
- в итоге приложение линкуется с библиотекой: 
```bash
$(APP): $(APP_OBJECTS) $(LIBRARY)
	$(CC) $(APP_OBJECTS) -L$(BUILD_DIR) -lphonebook -o $@
```

## Пример рузультата запуска make 
```bash
seger@GerasimLaptop:~/eltex_education_2026/task6.1_static_lib$ make
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -c main.c -o build/main.o
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -c src/phone_book_ui.c -o build/phone_book_ui.o
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -c src/phone_book.c -o build/phone_book.o
ar rcs build/libphonebook.a build/phone_book.o
gcc build/main.o build/phone_book_ui.o -Lbuild -lphonebook -o build/phone_book
```