# Задание 3 по модулю 5

Используя исходники модуля ядра 

- https://pastebin.com/r46SDJzs 
- https://pastebin.com/qEKTZZcB

Сделать мигание лампочек на клавиатуре используя ioctl. 

- Управлением миганием сделать через sysfs. 
- К примеру если записать в переменную в sysfs значение 1 горит первая лампочка, 2 горит вторая, 4 горит третья и т.д., 7-все. 
- Значение - это двоичная маска 1(001) 2(010) 7(111) .
- Результаты выложить на github или др. общедоступный git. Cсылку на git выслать в ЛС для проверки.

## Вспомогательные материалы:
- https://zen.yandex.ru/video/watch/627283f8dc7bc8449de12693
- https://zen.yandex.ru/video/watch/6272adbee508b5779954640c
- https://dzen.ru/video/watch/6272adbee508b5779954640c

## Реализация для Ubuntu

Модуль создает файл `/sys/kernel/kbleds/led_mask`. Записанное число задает
двоичную маску стандартных индикаторов клавиатуры:

| Значение | Индикатор |
|----------|-----------|
| `1` | Scroll Lock |
| `2` | Num Lock |
| `4` | Caps Lock |
| `7` | все три |

Комбинированные значения (`3`, `5`, `6`) включают несколько индикаторов.
Значение `0` прекращает мигание и выключает LED. При выгрузке модуля
восстанавливается штатное управление индикаторами клавиатуры.

На ноутбуке может физически присутствовать только индикатор Caps Lock. При
этом модуль все равно передает драйверу все три стандартных бита.

### Сборка

```bash
cd ~/Documents/eltex_education_2026/05-linux-kernel-modules/03-keyboard-leds-sysfs
make
```

Команда `make` только собирает модуль. Подпись выполняется отдельно командой
`make sign`, использующей ключи из каталога `~/.local/share/module-signing`.

Готовый модуль находится в `build/kbleds.ko`. Проверка информации о нем:

```bash
modinfo ./build/kbleds.ko
```

Очистка результатов сборки:

```bash
make clean
```

### Подпись модуля

Проверить состояние Secure Boot:

```bash
mokutil --sb-state
```

Подпись собранного модуля:

```bash
make sign
```

Для другого каталога с ключами:

```bash
make SIGN_KEY_DIR=/путь/к/ключам
```

Проверка подписи:

```bash
modinfo ./build/kbleds.ko | grep -E 'signer|sig_key|sig_hashalgo'
```

### Запуск

Проверять физические индикаторы следует из виртуальной консоли Ubuntu
(`Ctrl+Alt+F3`), а не из терминала графического приложения:

```bash
sudo insmod ./build/kbleds.ko
lsmod | grep kbleds
cat /sys/kernel/kbleds/led_mask

echo 4 | sudo tee /sys/kernel/kbleds/led_mask
cat /sys/kernel/kbleds/led_mask
```

Значение `4` запускает мигание Caps Lock. Для проверки остальных комбинаций
можно записывать значения от `1` до `7`:

```bash
echo 1 | sudo tee /sys/kernel/kbleds/led_mask
echo 2 | sudo tee /sys/kernel/kbleds/led_mask
echo 7 | sudo tee /sys/kernel/kbleds/led_mask
```

Остановка мигания и выгрузка:

```bash
echo 0 | sudo tee /sys/kernel/kbleds/led_mask
sudo rmmod kbleds
lsmod | grep kbleds
```

Сообщения модуля:

```bash
sudo dmesg | tail -n 20
```
