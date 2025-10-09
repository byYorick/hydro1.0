#!/usr/bin/env python3
"""
Скрипт для чтения памяти ESP32-S3 через OpenOCD JTAG
Автоматически подключается к OpenOCD и читает память
"""

import telnetlib
import time
import sys
import subprocess
import os

class ESP32Reader:
    def __init__(self, host="localhost", port=4444):
        self.host = host
        self.port = port
        self.tn = None
        self.openocd_process = None
    
    def start_openocd(self):
        """Запускает OpenOCD в фоне"""
        print("Запуск OpenOCD...")
        try:
            self.openocd_process = subprocess.Popen(
                ["openocd", "-f", "board/esp32s3-builtin.cfg"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NEW_CONSOLE if os.name == 'nt' else 0
            )
            time.sleep(3)  # Ждем инициализации
            print("✓ OpenOCD запущен")
            return True
        except Exception as e:
            print(f"✗ Ошибка запуска OpenOCD: {e}")
            return False
    
    def connect(self):
        """Подключается к OpenOCD через telnet"""
        try:
            print(f"Подключение к OpenOCD ({self.host}:{self.port})...")
            self.tn = telnetlib.Telnet(self.host, self.port, timeout=5)
            time.sleep(0.5)
            # Читаем приветствие
            self.tn.read_very_eager()
            print("✓ Подключено к OpenOCD")
            return True
        except Exception as e:
            print(f"✗ Ошибка подключения: {e}")
            return False
    
    def send_command(self, command):
        """Отправляет команду OpenOCD и возвращает результат"""
        try:
            self.tn.write(f"{command}\n".encode('ascii'))
            time.sleep(0.2)
            response = self.tn.read_very_eager().decode('ascii', errors='ignore')
            return response
        except Exception as e:
            print(f"✗ Ошибка выполнения команды: {e}")
            return None
    
    def read_memory(self, address, count=32, word_size='w'):
        """
        Читает память
        address: адрес в hex (например, 0x3FC88000)
        count: количество единиц для чтения
        word_size: 'b' (byte), 'h' (halfword), 'w' (word)
        """
        cmd = f"md{word_size} 0x{address:08X} {count}"
        print(f"\nЧтение: {cmd}")
        response = self.send_command(cmd)
        return response
    
    def get_chip_info(self):
        """Получает информацию о чипе"""
        print("\n=== Информация о чипе ===")
        response = self.send_command("esp chip_info")
        return response
    
    def get_registers(self):
        """Читает регистры процессора"""
        print("\n=== Регистры процессора ===")
        response = self.send_command("reg")
        return response
    
    def reset_halt(self):
        """Останавливает чип"""
        print("\nОстановка чипа...")
        return self.send_command("reset halt")
    
    def reset_run(self):
        """Запускает чип"""
        print("\nЗапуск чипа...")
        return self.send_command("reset run")
    
    def dump_flash(self, filename="flash_dump.bin", offset=0, size=0x1000):
        """Сохраняет дамп flash в файл"""
        print(f"\nСохранение flash (0x{offset:X}, размер 0x{size:X}) в {filename}...")
        cmd = f"flash read_bank 0 {filename} 0x{offset:X} 0x{size:X}"
        response = self.send_command(cmd)
        return response
    
    def close(self):
        """Закрывает соединение"""
        if self.tn:
            self.tn.close()
        if self.openocd_process:
            self.openocd_process.terminate()
        print("\n✓ Соединение закрыто")


def print_menu():
    print("\n" + "="*50)
    print("  ESP32-S3 JTAG Reader")
    print("="*50)
    print("1. Информация о чипе")
    print("2. Прочитать регистры")
    print("3. Прочитать RAM (DRAM: 0x3FC88000)")
    print("4. Прочитать IRAM (0x40370000)")
    print("5. Дамп Flash (bootloader)")
    print("6. Остановить чип (reset halt)")
    print("7. Запустить чип (reset run)")
    print("8. Своя команда OpenOCD")
    print("9. Прочитать произвольный адрес")
    print("0. Выход")
    print("="*50)


def main():
    print("ESP32-S3 JTAG Memory Reader")
    print("="*50)
    
    reader = ESP32Reader()
    
    # Запускаем OpenOCD
    if not reader.start_openocd():
        print("Не удалось запустить OpenOCD")
        print("Убедитесь что:")
        print("1. ESP32-S3 подключен через USB")
        print("2. OpenOCD доступен в PATH")
        print("3. Драйвер USB-JTAG установлен")
        return
    
    # Подключаемся
    if not reader.connect():
        print("Не удалось подключиться к OpenOCD")
        reader.close()
        return
    
    # Инициализация
    reader.send_command("init")
    
    try:
        while True:
            print_menu()
            choice = input("\nВыберите действие: ").strip()
            
            if choice == '1':
                result = reader.get_chip_info()
                print(result)
            
            elif choice == '2':
                result = reader.get_registers()
                print(result)
            
            elif choice == '3':
                result = reader.read_memory(0x3FC88000, 64, 'w')
                print(result)
            
            elif choice == '4':
                result = reader.read_memory(0x40370000, 64, 'w')
                print(result)
            
            elif choice == '5':
                result = reader.dump_flash("flash_bootloader.bin", 0, 0x10000)
                print(result)
                print("✓ Bootloader сохранен в flash_bootloader.bin")
            
            elif choice == '6':
                result = reader.reset_halt()
                print(result)
            
            elif choice == '7':
                result = reader.reset_run()
                print(result)
            
            elif choice == '8':
                cmd = input("Введите команду OpenOCD: ").strip()
                result = reader.send_command(cmd)
                print(result)
            
            elif choice == '9':
                addr_str = input("Введите адрес (hex, например 3FC88000): ").strip()
                try:
                    addr = int(addr_str, 16)
                    count = int(input("Количество слов (по умолчанию 32): ").strip() or "32")
                    result = reader.read_memory(addr, count, 'w')
                    print(result)
                except ValueError:
                    print("✗ Неверный формат адреса")
            
            elif choice == '0':
                break
            
            else:
                print("Неверный выбор")
    
    except KeyboardInterrupt:
        print("\n\nПрервано пользователем")
    
    finally:
        reader.close()
        print("Завершение работы")


if __name__ == "__main__":
    main()

