#!/usr/bin/env python3
"""
Анализатор состояния ESP32-S3 по дампу регистров
Помогает понять где находится программа и что происходит
"""

import re
import subprocess
import os

class ESP32StateAnalyzer:
    def __init__(self, elf_file="build/hydroponics.elf"):
        self.elf_file = elf_file
        self.pc = None
        self.sp = None
        self.registers = {}
        
    def parse_register_dump(self, dump_text):
        """Парсит дамп регистров из текста"""
        # Регулярка для парсинга: (N) name (/bits): 0xVALUE
        pattern = r'\((\d+)\)\s+(\w+)\s+\(/\d+\):\s+(0x[0-9a-fA-F]+)'
        
        for match in re.finditer(pattern, dump_text):
            reg_num = int(match.group(1))
            reg_name = match.group(2)
            reg_value = int(match.group(3), 16)
            self.registers[reg_name] = reg_value
            
        # Ключевые регистры
        self.pc = self.registers.get('pc', 0)
        self.sp = self.registers.get('a1', 0)  # a1 = SP в Xtensa
        
        print(f"[OK] Распознано {len(self.registers)} регистров")
        return self.registers
    
    def analyze_pc(self):
        """Анализирует Program Counter"""
        print("\n" + "="*60)
        print("📍 PROGRAM COUNTER ANALYSIS")
        print("="*60)
        
        pc = self.pc
        print(f"PC = 0x{pc:08X}")
        
        # Определяем область памяти
        if 0x40000000 <= pc < 0x40060000:
            region = "ROM (Boot ROM)"
            print(f"Область: {region} [!]")
            print("Программа выполняется в Boot ROM - возможно загрузка или исключение")
        elif 0x40370000 <= pc < 0x403E0000:
            region = "IRAM"
            print(f"Область: {region} [OK]")
            print("Программа выполняется из IRAM - нормально")
        elif 0x42000000 <= pc < 0x44000000:
            region = "Flash (memory mapped)"
            print(f"Область: {region} [OK]")
            print("Программа выполняется из Flash - нормально")
        elif 0x3FC88000 <= pc < 0x3FD00000:
            region = "DRAM"
            print(f"Область: {region} [!]")
            print("Выполнение из DRAM - необычно, возможно JIT или ошибка")
        else:
            region = "Unknown"
            print(f"Область: {region} [ERROR]")
            print("Неизвестная область памяти - возможна ошибка!")
        
        return region
    
    def analyze_sp(self):
        """Анализирует Stack Pointer"""
        print("\n" + "="*60)
        print("📚 STACK POINTER ANALYSIS")
        print("="*60)
        
        sp = self.sp
        print(f"SP = 0x{sp:08X}")
        
        # Проверяем что SP в правильной области
        if 0x3FC88000 <= sp < 0x3FD00000:
            print("Область: DRAM [OK]")
            print("Stack находится в правильной области")
            
            # Прикидываем использование стека (если знаем начало)
            # Обычно стек для задач FreeRTOS начинается в конце выделенной области
            # и растет вниз
        elif 0x3FCE0000 <= sp < 0x3FD00000:
            print("Область: DRAM (верхняя часть) [OK]")
        else:
            print("Область: [!] Необычная область для стека!")
            
        return sp
    
    def analyze_exceptions(self):
        """Анализирует регистры исключений"""
        print("\n" + "="*60)
        print("⚠️  EXCEPTION ANALYSIS")
        print("="*60)
        
        exccause = self.registers.get('exccause', 0)
        debugcause = self.registers.get('debugcause', 0)
        excvaddr = self.registers.get('excvaddr', 0)
        
        print(f"EXCCAUSE   = 0x{exccause:08X}")
        if exccause == 0:
            print("  -> Нет исключения [OK]")
        else:
            causes = {
                0: "IllegalInstructionCause",
                1: "SyscallCause", 
                2: "InstructionFetchErrorCause",
                3: "LoadStoreErrorCause",
                4: "Level1InterruptCause",
                5: "AllocaCause",
                6: "IntegerDivideByZeroCause",
                9: "LoadStoreAlignmentCause",
                28: "LoadProhibitedCause",
                29: "StoreProhibitedCause",
            }
            cause_str = causes.get(exccause, "Unknown")
            print(f"  -> {cause_str} [!]")
        
        print(f"\nDEBUGCAUSE = 0x{debugcause:08X}")
        if debugcause & 0x20:
            print("  -> Debug Interrupt (остановлен отладчиком)")
        if debugcause & 0x01:
            print("  -> Instruction breakpoint")
        if debugcause & 0x02:
            print("  -> Data breakpoint (load)")
        if debugcause & 0x04:
            print("  -> Data breakpoint (store)")
            
        if excvaddr != 0:
            print(f"\nEXCVADDR   = 0x{excvaddr:08X}")
            print("  -> Адрес, вызвавший исключение")
    
    def analyze_interrupts(self):
        """Анализирует прерывания"""
        print("\n" + "="*60)
        print("[IRQ] INTERRUPT ANALYSIS")
        print("="*60)
        
        interrupt = self.registers.get('interrupt', 0)
        intenable = self.registers.get('intenable', 0)
        
        print(f"INTERRUPT  = 0x{interrupt:08X} (активные)")
        print(f"INTENABLE  = 0x{intenable:08X} (разрешенные)")
        
        if interrupt:
            print("\nАктивные прерывания:")
            for i in range(32):
                if interrupt & (1 << i):
                    int_names = {
                        6: "Timer",
                        15: "WiFi MAC",
                        16: "WiFi BB",
                        19: "UART0",
                        22: "I2C",
                    }
                    name = int_names.get(i, f"IRQ{i}")
                    print(f"  - Bit {i}: {name}")
        else:
            print("Нет активных прерываний")
    
    def find_function_at_pc(self):
        """Находит функцию по адресу PC используя addr2line"""
        print("\n" + "="*60)
        print("[FUNC] FUNCTION LOOKUP")
        print("="*60)
        
        if not os.path.exists(self.elf_file):
            print(f"[!] ELF файл не найден: {self.elf_file}")
            return
        
        try:
            # Используем addr2line для поиска функции
            result = subprocess.run(
                ['xtensa-esp32s3-elf-addr2line', '-e', self.elf_file, 
                 '-f', '-C', f'0x{self.pc:08X}'],
                capture_output=True, text=True, timeout=5
            )
            
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                if len(lines) >= 2:
                    function = lines[0]
                    location = lines[1]
                    print(f"Функция: {function}")
                    print(f"Файл:    {location}")
                else:
                    print("Не удалось определить функцию")
            else:
                print("[!] addr2line недоступен")
                
        except FileNotFoundError:
            print("[!] xtensa-esp32s3-elf-addr2line не найден в PATH")
        except Exception as e:
            print(f"[!] Ошибка: {e}")
    
    def dump_stack(self):
        """Показывает как прочитать стек через OpenOCD"""
        print("\n" + "="*60)
        print("[STACK] STACK DUMP COMMANDS")
        print("="*60)
        
        sp = self.sp
        print(f"Stack Pointer: 0x{sp:08X}")
        print("\nДля чтения стека выполните в OpenOCD (telnet localhost 4444):")
        print(f"  mdw 0x{sp:08X} 64")
        print("\nИли в GDB:")
        print(f"  x/64xw 0x{sp:08X}")
        print("\nИли используйте Python скрипт:")
        print(f"  python read_esp32_memory.py")
        print(f"  # Выберите опцию 9 и введите адрес: {sp:08X}")
    
    def generate_commands(self):
        """Генерирует команды для дальнейшего исследования"""
        print("\n" + "="*60)
        print("[NEXT] SUGGESTED NEXT STEPS")
        print("="*60)
        
        print("\n[1] Прочитать стек вызовов:")
        print("   python read_esp32_memory.py")
        print(f"   # Адрес: {self.sp:08X}, count: 128")
        
        print("\n[2] Получить backtrace в GDB:")
        print("   xtensa-esp32s3-elf-gdb build/hydroponics.elf")
        print("   (gdb) target remote localhost:3333")
        print("   (gdb) bt")
        
        print("\n[3] Прочитать код по PC:")
        print(f"   (gdb) x/10i 0x{self.pc:08X}")
        
        print("\n[4] Продолжить выполнение:")
        print("   В telnet: reset run")
        print("   В GDB: continue")
        
        print("\n[5] Установить breakpoint на app_main:")
        print("   (gdb) b app_main")
        print("   (gdb) monitor reset halt")
        print("   (gdb) c")


def main():
    # Устанавливаем UTF-8 для Windows консоли
    if os.name == 'nt':
        try:
            import sys
            sys.stdout.reconfigure(encoding='utf-8')
        except:
            pass
    
    print("="*60)
    print("  ESP32-S3 State Analyzer")
    print("="*60)
    
    # Читаем дамп из файла или используем встроенный
    import sys
    
    if len(sys.argv) > 1:
        dump_file = sys.argv[1]
        with open(dump_file, 'r') as f:
            dump_text = f.read()
    else:
        # Встроенный дамп из терминала
        dump_text = """
(0) pc (/32): 0x40056f62
(1) ar0 (/32): 0x000230c8
(81) a0 (/32): 0x80045101
(82) a1 (/32): 0x3fceb1f0
(126) debugcause (/32): 0x00000020
(160) epc1 (/32): 0x40044290
(165) epc6 (/32): 0x40056f62
(177) exccause (/32): 0x00000000
(181) interrupt (/32): 0x00018040
(184) intenable (/32): 0x00000000
(188) excvaddr (/32): 0x00000000
"""
    
    analyzer = ESP32StateAnalyzer()
    analyzer.parse_register_dump(dump_text)
    
    # Выполняем анализ
    analyzer.analyze_pc()
    analyzer.analyze_sp()
    analyzer.analyze_exceptions()
    analyzer.analyze_interrupts()
    analyzer.find_function_at_pc()
    analyzer.dump_stack()
    analyzer.generate_commands()
    
    print("\n" + "="*60)
    print("[OK] Анализ завершен!")
    print("="*60)


if __name__ == "__main__":
    main()

