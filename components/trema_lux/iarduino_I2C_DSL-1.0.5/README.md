[![](https://iarduino.ru/img/logo.svg)](https://iarduino.ru)[![](https://wiki.iarduino.ru/img/git-shop.svg?3)](https://iarduino.ru) [![](https://wiki.iarduino.ru/img/git-wiki.svg?2)](https://wiki.iarduino.ru) [![](https://wiki.iarduino.ru/img/git-lesson.svg?2)](https://lesson.iarduino.ru)[![](https://wiki.iarduino.ru/img/git-forum.svg?2)](http://forum.trema.ru)

# iarduino\_I2C\_DSL

**This is a library for Arduino IDE. It allows to work with [Lux Meter](https://iarduino.ru/shop/Sensory-Datchiki/datchik-osveschennosti-lyuksmetr-i2c-trema-modul-v2-0.html), module made by iArduino.ru**

**Данная библиотека для Arduino IDE позволяет работать с модулем [Датчик освещённости, люксметр](https://iarduino.ru/shop/Sensory-Datchiki/datchik-osveschennosti-lyuksmetr-i2c-trema-modul-v2-0.html) от [iArduino.ru](https://iarduino.ru)**

> Подробнее про установку библиотеки читайте в нашей [инструкции](https://wiki.iarduino.ru/page/Installing_libraries/).

> Подробнее про подключение к [Arduino UNO](https://iarduino.ru/shop/boards/arduino-uno-r3.html)/[Piranha UNO](https://iarduino.ru/shop/boards/piranha-uno-r3.html) читайте на нашей [wiki](https://wiki.iarduino.ru/page/DSL-trema-i2c/#h3_3)


| Модель | Ссылка на магазин |
|---|---|
| <p></p> <img src="https://wiki.iarduino.ru/img/resources/1215/1215.svg" width="100px"></img>| https://iarduino.ru/shop/Sensory-Datchiki/datchik-osveschennosti-lyuksmetr-i2c-trema-modul-v2-0.html |


## Назначение функций:

**Подключаем библиотеку** 

```C++
#include <iarduino_I2C_DSL> // Подключаем библиотеку iarduino_I2C_DSL для работы с модулем.
```

**Создаём объект** 

```C++
iarduino_I2C_DSL ОБЪЕКТ; // Создаём объект для работы с методами библиотеки без указания адреса модуля на шине I2C.
```

**Создаём объект** 

```C++
iarduino_I2C_DSL ОБЪЕКТ( АДРЕС ); // Создаём объект для работы с методами библиотеки указывая адрес модуля на шине I2C.
```

**Инициализация работы** 

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

**Получение версии** 

```C++
ОБЪЕКТ.getVersion(); // Получение версии прошивки модуля.
```

**Получение освещённости** 

```C++
ОБЪЕКТ.getLux(); // Получение освещённости в люксах.
```

**Получение коэффициента пульсаций света** 

```C++
ОБЪЕКТ.getPulsation(); // Получение коэффициента пульсаций света в %.
```

**Определение близости препятствий** 

```C++
ОБЪЕКТ.getProximity(); // Определение близости препятствий.
```

**Запрос флага изменеия освещённости** 

```C++
ОБЪЕКТ.getLuxChanged(); // Запрос флага указывающего на изменение освещённости.
```

**Установка порога чувствительности** 

```C++
ОБЪЕКТ.setLuxChange( ПОРОГ ); // Порог чувствительности к изменению освещённости.
```

**Усреднение значений** 

```C++
ОБЪЕКТ.setAveraging( УСРЕДНЕНИЕ ); // Усреднение значений освещённости и близости.
```

