[![](https://iarduino.ru/img/logo.svg)](https://iarduino.ru)[![](https://wiki.iarduino.ru/img/git-shop.svg?3)](https://iarduino.ru) [![](https://wiki.iarduino.ru/img/git-wiki.svg?2)](https://wiki.iarduino.ru) [![](https://wiki.iarduino.ru/img/git-lesson.svg?2)](https://lesson.iarduino.ru)[![](https://wiki.iarduino.ru/img/git-forum.svg?2)](http://forum.trema.ru)

# iarduino\_I2C\_Expander

**This is a library for Arduino IDE. It allows to work with [Trema Expander](https://iarduino.ru/shop/Expansion-payments/rasshiritel-vyvodov-i2c---flash-trema-modul.html), module made by iArduino.ru**

**Данная библиотека для Arduino IDE позволяет работать с модулем [Расширитель выводов](https://iarduino.ru/shop/Expansion-payments/rasshiritel-vyvodov-i2c---flash-trema-modul.html) от [iArduino.ru](https://iarduino.ru)**

> Подробнее про установку библиотеки читайте в нашей [инструкции](https://wiki.iarduino.ru/page/Installing_libraries/).

> Подробнее про подключение к [Arduino UNO](https://iarduino.ru/shop/boards/arduino-uno-r3.html)/[Piranha UNO](https://iarduino.ru/shop/boards/piranha-uno-r3.html) читайте на нашей [wiki](https://wiki.iarduino.ru/page/trema-extender/#h3_3)


| Модель | Ссылка на магазин |
|---|---|
| <p></p> <img src="https://wiki.iarduino.ru/img/resources/1224/1224.svg" width="100px"></img>| https://iarduino.ru/shop/Expansion-payments/rasshiritel-vyvodov-i2c---flash-trema-modul.html |

Библиотека iarduino\_I2C\_Expander позволяет работать с модулем - Расширитель выводов, I2C-flash, конфигурировать выводы на вход или выход, читать аналоговые или цифровые уровни, задавать логические значения или сигналы ШИМ, управлять сервоприводами.


## Назначение функций:

**Подключаем библиотеку** 

```C++
#include <iarduino_I2C_Expander> // Подключаем библиотеку iarduino_I2C_Expander для работы с модулем.
```

**Создаём объект** 

```C++
iarduino_I2C_Expander ОБЪЕКТ; // Создаём объект для работы с методами библиотеки без указания адреса модуля на шине I2C.
```

**Создаём объект** 

```C++
iarduino_I2C_Expander ОБЪЕКТ( АДРЕС ); // Создаём объект для работы с методами библиотеки указывая адрес модуля на шине I2C.
```

**Инициализация модуля** 

```C++
ОБЪЕКТ.begin(); // Инициализация работы с модулем.
```

**Изменение адреса** 

```C++
ОБЪЕКТ.changeAddress( АДРЕС ); // Изменение адреса модуля на шине I2C.
```

**Перезагрузка модуля**

```C++
ОБЪЕКТ.reset(); // Перезагрузка модуля.
```

**Получение текущего адреса** 

```C++
ОБЪЕКТ.getAddress(); // Получение текущего адреса модуля на шине I2C.
```

**Получение версии прошивки** 

```C++
ОБЪЕКТ.getVersion(); // Получение версии прошивки модуля.
```

**Конфигурирование вывода**

```C++
ОБЪЕКТ.pinMode( ВЫВОД , НАПРАВЛЕНИЕ [, ТИП ] ); // Конфигурирование вывода.
```

**Внутрисхемное подключение резистора** 

```C++
ОБЪЕКТ.pinPull( ВЫВОД , РЕЗИСТОР ); // Внутрисхемное подключение резистора к выводу.
```

**Выбор внутренней схемы выхода** 

```C++
ОБЪЕКТ.pinOutScheme( ВЫВОД , СХЕМА ); // Выбор внутренней схемы включения выхода.
```

**Чтение логического уровня** 

```C++
ОБЪЕКТ.digitalRead( ВЫВОД ); // Чтение логического уровня.
```

**Установка логического уровня** 

```C++
ОБЪЕКТ.digitalWrite( ВЫВОД , УРОВЕНЬ ); // Установка логического уровня.
```

**Чтение аналогового уровня** 

```C++
ОБЪЕКТ.analogRead( ВЫВОД ); // Чтение аналогового (АЦП) уровня.
```

**Установка аналогового уровня** 

```C++
ОБЪЕКТ.analogWrite( ВЫВОД , УРОВЕНЬ ); // Установка аналогового (ШИМ) уровня.
```

**Установка коэффициента усреднения АЦП** 

```C++
ОБЪЕКТ.analogAveraging( УСРЕДНЕНИЕ ); // Установка коэффициента усреднения АЦП.
```

**Чтение логического уровня аланогового входа** 

```C++
ОБЪЕКТ.levelRead( ВЫВОД ); // Чтение логического уровня с аналогового входа.
```

**Указание границы АЦП для levelRead()** 

```C++
ОБЪЕКТ.levelWrite( ГРАНИЦА ); // Указание границы АЦП для функции levelRead().
```

**Указание гистерезиса levelRead()** 

```C++
ОБЪЕКТ.levelHyst( ГИСТЕРЕЗИС ); // Указание гистерезиса для функции levelRead().
```

**Установка частоты ШИМ** 

```C++
ОБЪЕКТ.freqPWM( ЧАСТОТА ); // Установка частоты ШИМ.
```

**Настройка сервоприводов**

```C++
ОБЪЕКТ.servoAttach( № , W_MIN , W_MAX [, A_MIN , A_MAX ] ); // Настройка сервоприводов.
```

**Поворот ротора на угол** 

```C++
ОБЪЕКТ.servoWrite( ВЫВОД , УГОЛ ); // Поворот ротора сервопривода в указанный угол.
```

**Поворот ротора** 

```C++
ОБЪЕКТ.servoWriteMicroseconds( ВЫВОД , ШИРИНА ); // Поворот ротора сервопривода.
```

