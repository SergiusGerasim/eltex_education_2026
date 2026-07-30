# Задание 6.3
Доработать решение задачи 2.3 (калькулятор) так, чтобы
функции загружались из динамических библиотек. В одной библиотеке
находится одна функция. При запуске программы считывается каталог с
библиотеками и загружаются найденные функции.

## запуск make:

```bash
seger@GerasimLaptop:~/eltex_education_2026/task6.3_calc_dynamic_libS$ make
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -MMD -MP -c main.c -o build/main.o
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -MMD -MP -c src/calculator.c -o build/src/calculator.o
gcc  build/main.o build/src/calculator.o -lm -ldl -o build/calculator
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -shared plugins/add.c -lm -o build/plugins/add.so
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -shared plugins/devide.c -lm -o build/plugins/devide.so
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -shared plugins/diff.c -lm -o build/plugins/diff.so
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -shared plugins/pow.c -lm -o build/plugins/pow.so
gcc -Iinclude -I../libs/Unity/src -DUNITY_INCLUDE_DOUBLE -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -shared plugins/times.c -lm -o build/plugins/times.so
```

## результат работы тестов:

```bash
seger@GerasimLaptop:~/eltex_education_2026/task6.3_calc_dynamic_libS$ make test
./build/test_calculator ./build/plugins
tests/test_calculator.c:88:test_dynamic_libraries_should_load_all_plugins:PASS
tests/test_calculator.c:89:test_functions_from_dynamic_libraries_should_work:PASS
tests/test_calculator.c:90:test_calculator_should_use_loaded_plugin_functions:PASS
tests/test_calculator.c:91:test_dynamic_libraries_should_unload:PASS

-----------------------
4 Tests 0 Failures 0 Ignored 
OK
```