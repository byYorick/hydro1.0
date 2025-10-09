
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Скрипт для завершения процессов, использующих COM3 порт
"""

import subprocess
import sys
import os
import signal
import psutil

def find_com3_processes():
    """Найти процессы, использующие COM3 порт"""
    com3_processes = []
    
    try:
        # Поиск через netstat
        result = subprocess.run(['netstat', '-ano'], capture_output=True, text=True, shell=True)
        lines = result.stdout.split('\n')
        
        for line in lines:
            if 'COM3' in line or ':COM3' in line:
                parts = line.split()
                if len(parts) >= 5:
                    try:
                        pid = int(parts[-1])
                        com3_processes.append(pid)
                        print(f"Найден процесс с PID {pid}: {line.strip()}")
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Ошибка при поиске через netstat: {e}")
    
    return com3_processes

def kill_process(pid):
    """Завершить процесс по PID"""
    try:
        process = psutil.Process(pid)
        process_name = process.name()
        print(f"Завершение процесса: {process_name} (PID: {pid})")
        
        # Попробуем мягкое завершение
        process.terminate()
        process.wait(timeout=5)
        print(f"Процесс {pid} успешно завершен")
        return True
        
    except psutil.TimeoutExpired: 
        # Если мягкое завершение не сработало, принудительно
        try:
            process.kill()
            print(f"Процесс {pid} принудительно завершен")
            return True
        except Exception as e:
            print(f"Ошибка при принудительном завершении процесса {pid}: {e}")
            return False
            
    except psutil.NoSuchProcess:
        print(f"Процесс {pid} уже не существует")
        return True
        
    except Exception as e:
        print(f"Ошибка при завершении процесса {pid}: {e}")
        return False

def find_serial_related_processes():
    """Найти процессы, которые могут использовать COM порты"""
    serial_keywords = ['serial', 'com', 'esp', 'idf', 'monitor', 'putty', 'teraterm', 'arduino']
    related_processes = []
    
    for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
        try:
            proc_info = proc.info
            proc_name = proc_info['name'].lower()
            cmdline = ' '.join(proc_info['cmdline'] or []).lower()
            
            for keyword in serial_keywords:
                if keyword in proc_name or keyword in cmdline:
                    related_processes.append((proc_info['pid'], proc_info['name']))
                    break
                    
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            continue
    
    return related_processes

def main():
    print("=== Скрипт завершения процессов COM3 ===\n")
    
    # Поиск процессов, использующих COM3
    print("1. Поиск процессов, использующих COM3...")
    com3_pids = find_com3_processes()
    
    if com3_pids:
        print(f"\nНайдено {len(com3_pids)} процессов, использующих COM3")
        for pid in com3_pids:
            kill_process(pid)
    else:
        print("Процессы, использующие COM3, не найдены")
    
    # Поиск связанных процессов
    print("\n2. Поиск связанных с COM портами процессов...")
    related_processes = find_serial_related_processes()
    
    if related_processes:
        print(f"\nНайдено {len(related_processes)} потенциально связанных процессов:")
        for pid, name in related_processes:
            print(f"  {name} (PID: {pid})")
        
        response = input("\nЗавершить все связанные процессы? (y/n): ").lower()
        if response == 'y':
            for pid, name in related_processes:
                kill_process(pid)
    
    # Финальная проверка
    print("\n3. Финальная проверка...")
    final_check = find_com3_processes()
    if final_check:
        print("❌ COM3 все еще занят следующими процессами:")
        for pid in final_check:
            try:
                proc = psutil.Process(pid)
                print(f"  {proc.name()} (PID: {pid})")
            except:
                print(f"  Неизвестный процесс (PID: {pid})")
    else:
        print("✅ COM3 порт свободен!")
    
    print("\nСкрипт завершен.")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nСкрипт прерван пользователем")
    except Exception as e:
        print(f"\nОшибка: {e}")
        sys.exit(1)
