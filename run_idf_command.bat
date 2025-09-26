@echo off
REM Launch ESP-IDF terminal and execute command
REM Usage: run_idf_command.bat [command]
REM If no command is specified, it will just open the terminal

setlocal

REM Initialize ESP-IDF environment
call "C:\Espressif\idf_cmd_init.bat" esp-idf-ab7213b7273352b64422b1f400ff27a0

REM If a command was provided, execute it
if "%1" neq "" (
    %*
) else (
    REM Just keep the terminal open
    cmd /k
)

endlocal