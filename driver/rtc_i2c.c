#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/bcd.h>

#define HT74563A_I2C_ADDR 0x51  // I2C адрес микросхемы HT74563A (7-битный адрес)

// Регистры HT74563A (согласно даташиту)
#define HT74563A_REG_SECONDS   0x02
#define HT74563A_REG_MINUTES   0x03
#define HT74563A_REG_HOURS     0x04
#define HT74563A_REG_DATE      0x05  // День месяца
#define HT74563A_REG_DAY       0x06  // День недели
#define HT74563A_REG_MONTH     0x07
#define HT74563A_REG_YEAR      0x08
#define HT74563A_REG_CONTROL   0x00
#define HT74563A_REG_SQW       0x0D

// Чтение регистра
static int rtc_i2c_read_reg(struct i2c_client *client, u8 reg, u8 *value)
{
    int ret;
    ret = i2c_smbus_read_byte_data(client, reg);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read from RTC register 0x%02x\n", reg);
        return ret;
    }
    *value = ret;
    return 0;
}

// Запись в регистр
static int rtc_i2c_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
    int ret;
    ret = i2c_smbus_write_byte_data(client, reg, value);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to write to RTC register 0x%02x\n", reg);
        return ret;
    }
    return 0;
}

// Чтение времени из RTC
static int rtc_i2c_read_time(struct device *dev, struct rtc_time *tm)
{
    struct i2c_client *client = to_i2c_client(dev);
    u8 regs[7];
    int ret;

    // Чтение всех регистров времени (от секунд до года)
    for (int i = 0; i < 7; i++) {
        ret = rtc_i2c_read_reg(client, HT74563A_REG_SECONDS + i, ®s[i]);
        if (ret < 0) {
            dev_err(dev, "Failed to read time from RTC\n");
            return ret;
        }
    }

    /*
     * Опционально: Проверка флага остановки осциллятора (OSF) в регистре секунд.
     * Если бит 7 установлен, осциллятор останавливался, и время может быть
     * недействительным. Можно вывести предупреждение без прерывания чтения
     * или вернуть ошибку для уведомления верхних уровней.
     *
     * if (regs[0] & 0x80) {
     *     dev_warn(dev, "Обнаружена остановка осциллятора, время может быть недействительным\n");
     *     // Опционально: return -EINVAL;
     * }
     */

    // Активная проверка OSF (можно закомментировать, если нужно)
    if (regs[0] & 0x80) {
        dev_warn(dev, "Oscillator stop detected, time may be invalid\n");
    }

    // Преобразование BCD в двоичный формат с учетом масок из даташита
    tm->tm_sec  = bcd2bin(regs[0] & 0x7F);  // Секунды (00-59), бит 7 — OSF
    tm->tm_min  = bcd2bin(regs[1] & 0x7F);  // Минуты (00-59)
    tm->tm_hour = bcd2bin(regs[2] & 0x3F);  // Часы (00-23, 24-часовой формат)
    tm->tm_mday = bcd2bin(regs[3] & 0x3F);  // День месяца (01-31)
    tm->tm_wday = bcd2bin(regs[4] & 0x07);  // День недели (00-06)
    tm->tm_mon  = bcd2bin(regs[5] & 0x1F) - 1;  // Месяц (01-12, в ядре 0-11)
    tm->tm_year = bcd2bin(regs[6] & 0xFF) + 100;  // Год (00-99, относительно 2000)

    return 0;
}

// Запись времени в RTC
static int rtc_i2c_set_time(struct device *dev, struct rtc_time *tm)
{
    struct i2c_client *client = to_i2c_client(dev);
    u8 regs[7];
    int ret;

    // Преобразование двоичного формата в BCD
    regs[0] = bin2bcd(tm->tm_sec);        // Секунды (00-59)
    regs[1] = bin2bcd(tm->tm_min);        // Минуты (00-59)
    regs[2] = bin2bcd(tm->tm_hour);       // Часы (00-23)
    regs[3] = bin2bcd(tm->tm_mday);       // День месяца (01-31)
    regs[4] = bin2bcd(tm->tm_wday);       // День недели (00-06)
    regs[5] = bin2bcd(tm->tm_mon + 1);    // Месяц (1-12 для RTC)
    regs[6] = bin2bcd(tm->tm_year - 100); // Год (00-99 относительно 2000)

    // Запись всех регистров времени
    for (int i = 0; i < 7; i++) {
        ret = rtc_i2c_write_reg(client, HT74563A_REG_SECONDS + i, regs[i]);
        if (ret < 0) {
            dev_err(dev, "Failed to write time to RTC\n");
            return ret;
        }
    }

    return 0;
}

// Операции RTC
static const struct rtc_class_ops rtc_i2c_ops = {
    .read_time  = rtc_i2c_read_time,
    .set_time   = rtc_i2c_set_time,
};

// Инициализация драйвера
static int rtc_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct rtc_device *rtc;
    int ret;

    // Проверка наличия устройства
    u8 value;
    ret = rtc_i2c_read_reg(client, HT74563A_REG_SECONDS, &value);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to probe RTC\n");
        return ret;
    }

    // Регистрация RTC устройства
    rtc = devm_rtc_device_register(&client->dev, "ht74563a", &rtc_i2c_ops, THIS_MODULE);
    if (IS_ERR(rtc)) {
        dev_err(&client->dev, "Failed to register RTC device\n");
        return PTR_ERR(rtc);
    }

    dev_info(&client->dev, "HT74563A RTC probed successfully\n");
    return 0;
}

// Удаление драйвера
static int rtc_i2c_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "HT74563A RTC removed\n");
    return 0;
}

// Таблица совместимости для Device Tree
static const struct of_device_id rtc_i2c_of_match[] = {
    { .compatible = "htsemi,ht74563a" },
    { }
};
MODULE_DEVICE_TABLE(of, rtc_i2c_of_match);

// Таблица идентификаторов для I2C
static const struct i2c_device_id rtc_i2c_id[] = {
    { "ht74563a", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, rtc_i2c_id);

// Структура драйвера
static struct i2c_driver rtc_i2c_driver = {
    .driver = {
        .name = "ht74563a",
        .of_match_table = rtc_i2c_of_match,
    },
    .probe = rtc_i2c_probe,
    .remove = rtc_i2c_remove,
    .id_table = rtc_i2c_id,
};

module_i2c_driver(rtc_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serg");
MODULE_DESCRIPTION("HT74563A I2C RTC Driver");
