#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Скрипт для автоматического освобождения COM порта
Использование: python free_com_port.py COM10
"""

import subprocess
import sys
import os
import time

def kill_process_by_pid(pid):
    """Завершить процесс по PID"""
    try:
        result = subprocess.run(
            ['taskkill', '/F', '/PID', str(pid)],
            capture_output=True,
            text=True,
            encoding='cp866',
            errors='ignore'
        )
        return result.returncode == 0
    except:
        return False

def find_and_kill_serial_processes(port_name):
    """Найти и завершить процессы, связанные с COM портами"""
    
    print(f"Поиск процессов, использующих {port_name}...\n")
    
    # Получаем текущий PID (не завершаем сами себя!)
    current_pid = os.getpid()
    
    # Получаем список всех процессов
    try:
        result = subprocess.run(
            ['tasklist', '/V', '/FO', 'CSV'],
            capture_output=True,
            text=True,
            encoding='cp866',
            errors='ignore'
        )
        
        lines = result.stdout.split('\n')
        
        # Ключевые слова для поиска
        keywords = ['idf.py', 'monitor', 'esptool', 'putty', 'teraterm', 'arduino']
        
        killed = 0
        total = 0
        skipped_self = 0
        
        for line in lines:
            if not line.strip():
                continue
            
            # Парсим CSV (формат: "Name","PID","Session","Memory","Status","User","CPU","Window")
            parts = line.split('","')
            if len(parts) < 2:
                continue
            
            name = parts[0].strip('"').lower()
            
            # Проверяем по ключевым словам
            should_kill = False
            for keyword in keywords:
                if keyword.lower() in name:
                    should_kill = True
                    break
            
            if should_kill:
                try:
                    pid = int(parts[1].strip('"'))
                    
                    # Не завершаем сами себя и родительский процесс
                    if pid == current_pid or pid == os.getppid():
                        skipped_self += 1
                        continue
                    
                    proc_name = parts[0].strip('"')
                    total += 1
                    print(f"  [{total}] Завершаю: {proc_name} (PID: {pid})")
                    
                    if kill_process_by_pid(pid):
                        killed += 1
                        print(f"      [OK] Завершен")
                    else:
                        print(f"      [SKIP] Уже завершен или нет доступа")
                        
                except (ValueError, IndexError):
                    continue
        
        if total > 0:
            print(f"\n[OK] Завершено {killed} из {total} процессов")
        else:
            print(f"[OK] Процессы не найдены")
        
        # Задержка для освобождения ресурсов
        time.sleep(1)
        return True
        
    except Exception as e:
        print(f"[ERROR] Ошибка: {e}")
        return False

def main():
    # Проверка аргументов
    if len(sys.argv) < 2:
        print("Использование: python free_com_port.py COM10")
        sys.exit(1)
    
    port_name = sys.argv[1].upper()
    
    print("=" * 60)
    print(f"  Освобождение порта {port_name}")
    print("=" * 60 + "\n")
    
    success = find_and_kill_serial_processes(port_name)
    
    print("\n" + "=" * 60)
    if success:
        print(f"  [SUCCESS] Порт {port_name} освобожден")
    else:
        print(f"  [FAIL] Не удалось освободить порт")
    print("=" * 60)
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n[!] Прервано пользователем")
        sys.exit(130)
    except Exception as e:
        print(f"\n[ERROR] {e}")
        sys.exit(1)
