#!/usr/bin/env python3
"""
Автоматический анализ crash report от ESP32-S3
Читает отчет и выдает структурированную информацию для отладки
"""

import sys
import re
import os

class CrashAnalyzer:
    def __init__(self, report_file):
        self.report_file = report_file
        self.report_data = ""
        self.pc = None
        self.sp = None
        self.backtrace = []
        self.registers = {}
        self.tasks = []
        
    def load_report(self):
        """Загружает файл отчета"""
        try:
            with open(self.report_file, 'r', encoding='utf-8', errors='ignore') as f:
                self.report_data = f.read()
            print(f"[OK] Загружен отчет: {self.report_file}")
            print(f"     Размер: {len(self.report_data)} байт")
            return True
        except FileNotFoundError:
            print(f"[ERROR] Файл не найден: {self.report_file}")
            return False
        except Exception as e:
            print(f"[ERROR] Ошибка чтения: {e}")
            return False
    
    def parse_registers(self):
        """Парсит регистры из отчета"""
        print("\n" + "="*60)
        print("[1] АНАЛИЗ РЕГИСТРОВ")
        print("="*60)
        
        # Ищем PC
        pc_match = re.search(r'pc\s+(?:\(\/\d+\))?\s*(?:0x)?([0-9a-fA-F]+)', self.report_data)
        if pc_match:
            self.pc = int(pc_match.group(1), 16)
            print(f"\nProgram Counter (PC) = 0x{self.pc:08X}")
            self.analyze_pc_location(self.pc)
        else:
            print("\n[!] PC не найден в отчете")
        
        # Ищем SP (a1)
        sp_match = re.search(r'a1\s+(?:\(\/\d+\))?\s*(?:0x)?([0-9a-fA-F]+)', self.report_data)
        if sp_match:
            self.sp = int(sp_match.group(1), 16)
            print(f"\nStack Pointer (SP) = 0x{self.sp:08X}")
            self.analyze_sp_location(self.sp)
        else:
            print("\n[!] SP не найден в отчете")
        
        # Ищем EXCCAUSE
        exc_match = re.search(r'exccause\s+(?:\(\/\d+\))?\s*(?:0x)?([0-9a-fA-F]+)', self.report_data)
        if exc_match:
            exccause = int(exc_match.group(1), 16)
            print(f"\nEXCCAUSE = 0x{exccause:08X}")
            self.decode_exception(exccause)
    
    def analyze_pc_location(self, pc):
        """Определяет область памяти PC"""
        if 0x40000000 <= pc < 0x40060000:
            print("  -> Область: ROM (Boot ROM)")
            print("  -> [!] Программа в ROM - возможно исключение или загрузка")
        elif 0x40370000 <= pc < 0x403E0000:
            print("  -> Область: IRAM")
            print("  -> [OK] Нормальное выполнение из IRAM")
        elif 0x42000000 <= pc < 0x44000000:
            print("  -> Область: Flash (mapped)")
            print("  -> [OK] Нормальное выполнение из Flash")
        elif 0x3FC88000 <= pc < 0x3FD00000:
            print("  -> Область: DRAM")
            print("  -> [!] Выполнение из DRAM - необычно!")
        else:
            print(f"  -> [ERROR] Неизвестная область памяти!")
            print(f"  -> [!] Возможен серьезный баг (corruption, wild jump)")
    
    def analyze_sp_location(self, sp):
        """Проверяет корректность SP"""
        if 0x3FC88000 <= sp < 0x3FD00000:
            print("  -> Область: DRAM")
            print("  -> [OK] Stack в правильной области")
        else:
            print("  -> [ERROR] Stack вне допустимой области!")
            print("  -> [!] Возможен stack overflow или corruption")
    
    def decode_exception(self, exccause):
        """Декодирует причину исключения"""
        exceptions = {
            0: ("No exception", "OK"),
            1: ("IllegalInstructionCause", "Неверная инструкция"),
            2: ("InstructionFetchErrorCause", "Ошибка чтения инструкции"),
            3: ("LoadStoreErrorCause", "Ошибка доступа к памяти"),
            4: ("Level1InterruptCause", "Прерывание уровня 1"),
            5: ("AllocaCause", "Ошибка ALLOCA"),
            6: ("IntegerDivideByZeroCause", "Деление на ноль"),
            9: ("LoadStoreAlignmentCause", "Невыровненный доступ к памяти"),
            28: ("LoadProhibitedCause", "Запрещенное чтение"),
            29: ("StoreProhibitedCause", "Запрещенная запись"),
        }
        
        if exccause in exceptions:
            name, desc = exceptions[exccause]
            if exccause == 0:
                print(f"  -> {name}")
                print(f"  -> [OK] {desc}")
            else:
                print(f"  -> {name}")
                print(f"  -> [!] {desc}")
        else:
            print(f"  -> [!] Неизвестный код исключения: {exccause}")
    
    def parse_backtrace(self):
        """Парсит backtrace"""
        print("\n" + "="*60)
        print("[2] АНАЛИЗ BACKTRACE")
        print("="*60)
        
        # Ищем секцию backtrace
        bt_section = re.search(r'2\. BACKTRACE.*?\n(.*?)(?:\n\s*\n|\Z)', self.report_data, re.DOTALL)
        if bt_section:
            bt_text = bt_section.group(1)
            
            # Парсим строки вида: #0  0x40056f62 in function_name () at file.c:123
            frames = re.findall(r'#(\d+)\s+(0x[0-9a-fA-F]+)(?:\s+in\s+(.+?))?(?:\s+at\s+(.+))?$', 
                               bt_text, re.MULTILINE)
            
            if frames:
                print("\nСтек вызовов:")
                for num, addr, func, loc in frames:
                    print(f"  #{num:2s}  {addr}")
                    if func:
                        print(f"       -> {func}")
                    if loc:
                        print(f"       -> {loc}")
                    self.backtrace.append({
                        'frame': int(num),
                        'addr': addr,
                        'function': func or '??',
                        'location': loc or '??'
                    })
            else:
                print("\n[!] Backtrace не содержит распознаваемых фреймов")
                print("Возможные причины:")
                print("  - Поврежден стек")
                print("  - Нет символов отладки в .elf")
                print("  - Чип в состоянии до инициализации")
        else:
            print("\n[!] Секция BACKTRACE не найдена в отчете")
    
    def parse_tasks(self):
        """Парсит задачи FreeRTOS"""
        print("\n" + "="*60)
        print("[3] АНАЛИЗ ЗАДАЧ FREERTOS")
        print("="*60)
        
        # Ищем секцию tasks
        tasks_section = re.search(r'3\. FREERTOS TASKS.*?\n(.*?)(?:\n\s*\n|\Z)', 
                                 self.report_data, re.DOTALL)
        if tasks_section:
            tasks_text = tasks_section.group(1)
            
            # Парсим строки с задачами
            # Формат: * 1    Thread 0x... (LWP 1) "TaskName"  0x... in func ()
            task_lines = re.findall(r'Thread\s+(0x[0-9a-fA-F]+).*?"([^"]+)"', tasks_text)
            
            if task_lines:
                print(f"\nНайдено задач: {len(task_lines)}")
                for tid, name in task_lines:
                    print(f"  - {name} (TID: {tid})")
                    self.tasks.append({'name': name, 'tid': tid})
            else:
                print("\n[!] Задачи не найдены")
                print("Возможно FreeRTOS еще не запущен")
        else:
            print("\n[!] Секция FREERTOS TASKS не найдена")
    
    def analyze_stack_content(self):
        """Анализирует содержимое стека"""
        print("\n" + "="*60)
        print("[4] АНАЛИЗ СОДЕРЖИМОГО СТЕКА")
        print("="*60)
        
        # Ищем секцию stack dump
        stack_section = re.search(r'4\. STACK MEMORY DUMP.*?\n(.*?)(?:\n\s*\n|\Z)', 
                                 self.report_data, re.DOTALL)
        if stack_section:
            stack_text = stack_section.group(1)
            
            # Ищем адреса похожие на указатели на код
            code_ptrs = re.findall(r'(0x[48][0-9a-fA-F]{7})', stack_text)
            
            if code_ptrs:
                print(f"\nНайдено {len(code_ptrs)} указателей на код в стеке:")
                # Уникальные адреса
                unique_ptrs = list(set(code_ptrs))[:10]  # Первые 10
                for ptr in unique_ptrs:
                    addr = int(ptr, 16)
                    print(f"  {ptr}")
                    
                print("\n[i] Эти адреса могут быть return addresses в стеке вызовов")
            else:
                print("\n[!] Указатели на код не найдены в стеке")
        else:
            print("\n[!] Секция STACK MEMORY DUMP не найдена")
    
    def generate_summary(self):
        """Генерирует итоговую сводку"""
        print("\n" + "="*60)
        print("[SUMMARY] ИТОГОВАЯ СВОДКА")
        print("="*60)
        
        print("\n[Ключевая информация для передачи AI]")
        print("-" * 60)
        
        if self.pc:
            print(f"PC = 0x{self.pc:08X}")
        if self.sp:
            print(f"SP = 0x{self.sp:08X}")
        
        if self.backtrace:
            print(f"\nTop of backtrace ({len(self.backtrace)} frames):")
            for frame in self.backtrace[:5]:
                print(f"  #{frame['frame']} {frame['addr']} {frame['function']}")
                if frame['location'] != '??':
                    print(f"      at {frame['location']}")
        
        if self.tasks:
            print(f"\nFreeRTOS tasks ({len(self.tasks)}):")
            for task in self.tasks[:5]:
                print(f"  - {task['name']}")
        
        print("\n" + "-" * 60)
        print("\n[Рекомендации]")
        
        if self.pc and 0x40000000 <= self.pc < 0x40060000:
            print("  [!] PC в Boot ROM - возможно обработка исключения")
            print("      Проверьте backtrace на наличие вашего кода")
        
        if not self.backtrace:
            print("  [!] Нет backtrace - возможно поврежден стек")
            print("      Проверьте на stack overflow")
        
        print("\n[Как передать эти данные AI]")
        print("  1. Скопируйте весь файл:", self.report_file)
        print("  2. Или скопируйте эту SUMMARY секцию")
        print("  3. Вставьте в чат с описанием проблемы")
        print("  4. Укажите что происходило перед крашем")
    
    def analyze(self):
        """Выполняет полный анализ"""
        if not self.load_report():
            return False
        
        self.parse_registers()
        self.parse_backtrace()
        self.parse_tasks()
        self.analyze_stack_content()
        self.generate_summary()
        
        return True


def main():
    if os.name == 'nt':
        try:
            import sys
            sys.stdout.reconfigure(encoding='utf-8')
        except:
            pass
    
    print("="*60)
    print("  ESP32-S3 Crash Report Analyzer")
    print("="*60)
    
    if len(sys.argv) < 2:
        print("\nИспользование:")
        print("  python analyze_crash.py crash_report.txt")
        print("\nИли найдите последний отчет:")
        import glob
        reports = glob.glob("crash_report_*.txt")
        if reports:
            reports.sort(reverse=True)
            latest = reports[0]
            print(f"\nНайден последний отчет: {latest}")
            print(f"Запускаю анализ...\n")
            report_file = latest
        else:
            print("\n[!] Файлы отчетов не найдены")
            print("Запустите: collect_crash_data.bat")
            return
    else:
        report_file = sys.argv[1]
    
    analyzer = CrashAnalyzer(report_file)
    if analyzer.analyze():
        print("\n" + "="*60)
        print("[OK] Анализ завершен!")
        print("="*60)
        print("\nФайл отчета готов для передачи в чат.")
        print("Скопируйте содержимое и опишите проблему.")


if __name__ == "__main__":
    main()

