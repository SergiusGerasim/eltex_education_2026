# Задание 6.2:
Доработать решение задачи 6.1 (список контактов) так, чтобы структуры и
функции по работе с двухсвязным упорядоченным списком находились в
динамической библиотеке.


## Пример рузультата запуска make 
```bash
seger@GerasimLaptop:~/eltex_education_2026/task6.2_dynamic_lib$ make 
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -c main.c -o build/main.o
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -c src/phone_book_ui.c -o build/phone_book_ui.o
mkdir -p build
gcc -Iinclude -I../libs/Unity/src -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -fPIC -c src/phone_book.c -o build/phone_book.o
gcc -shared build/phone_book.o -o build/libphonebook.so
gcc build/main.o build/phone_book_ui.o -Lbuild -lphonebook -Wl,-rpath,'$ORIGIN' -o build/phone_book
```

## Проверка библиотеки:
```bash
seger@GerasimLaptop:~/eltex_education_2026/task6.2_dynamic_lib$ file build/libphonebook.so 
build/libphonebook.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, BuildID[sha1]=8a23fe87bf532f4263de05d372d989ce22bb9aad, not stripped
seger@GerasimLaptop:~/eltex_education_2026/task6.2_dynamic_lib$ ldd build/phone_book
        linux-vdso.so.1 (0x00007fffbaeef000)
        libphonebook.so => /home/seger/eltex_education_2026/task6.2_dynamic_lib/build/libphonebook.so (0x00007a1bb6638000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007a1bb6400000)
        /lib64/ld-linux-x86-64.so.2 (0x00007a1bb6646000)
seger@GerasimLaptop:~/eltex_education_2026/task6.2_dynamic_lib$ nm -D build/libphonebook.so 
                 w _ITM_deregisterTMCloneTable
                 w _ITM_registerTMCloneTable
                 w __cxa_finalize@GLIBC_2.2.5
                 w __gmon_start__
                 U __stack_chk_fail@GLIBC_2.4
                 U free@GLIBC_2.2.5
                 U malloc@GLIBC_2.2.5
000000000000205c T phone_book_clear
0000000000001f3a T phone_book_edit_email
0000000000001da1 T phone_book_edit_home_address
0000000000001e57 T phone_book_edit_job_title
0000000000001c53 T phone_book_edit_last_name
0000000000001bfb T phone_book_edit_name
0000000000001d46 T phone_book_edit_patronymic
0000000000001eb2 T phone_book_edit_phone
0000000000001fcb T phone_book_edit_social_link
0000000000001dfc T phone_book_edit_workplace
0000000000001b8b T phone_book_find_by_id
000000000000232c T phone_book_find_by_last_name
00000000000022fe T phone_book_find_by_name
0000000000002357 T phone_book_find_by_name_and_last_name
0000000000001946 T phone_book_id_manager_init
00000000000015b1 T phone_book_node_add_email
000000000000150d T phone_book_node_add_phone
000000000000165e T phone_book_node_add_social_link
0000000000001a87 T phone_book_node_create_and_add
0000000000002100 T phone_book_remove
0000000000002388 T phone_book_search_result_free
                 U realloc@GLIBC_2.2.5
                 U snprintf@GLIBC_2.2.5
                 U strcmp@GLIBC_2.2.5
                 U strlen@GLIBC_2.2.5
```