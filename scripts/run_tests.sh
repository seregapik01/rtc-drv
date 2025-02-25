#!/bin/bash

# Запуск эмулятора RTC в фоновом режиме
python3 /workspace/scripts/rtc_emulator.py &

# Даем эмулятору время на запуск
sleep 2

# Загрузка драйвера
sudo insmod /workspace/driver/rtc_i2c.ko

# Проверка, что устройство появилось
if [ -e /dev/rtc0 ]; then
    echo "RTC device found at /dev/rtc0"
else
    echo "RTC device not found!"
    exit 1
fi

# Установка времени
sudo hwclock --set --date="2025-02-26 12:00:00" --rtc=/dev/rtc0

# Чтение времени
sudo hwclock -r -f /dev/rtc0

# Удаление драйвера
sudo rmmod rtc_i2c

# Остановка эмулятора
pkill -f rtc_emulator.py

echo "Tests completed successfully"
exit 0