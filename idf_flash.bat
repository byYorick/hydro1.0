@echo off
call C:\Espressif\idf_cmd_init.bat esp-idf-1dcc643656a1439837fdf6ab63363005
cd /d C:\esp\hydro\hydro1.0
idf.py -p COM5 flash monitor

