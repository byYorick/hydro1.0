param(
    [string]$Command = "idf.py build"
)

# Set up ESP-IDF environment variables
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.3.1"
$env:PATH = "C:\Espressif\tools\xtensa-esp-elf-gdb\14.2_20240403\xtensa-esp-elf-gdb\bin;" +
            "C:\Espressif\tools\riscv32-esp-elf-gdb\14.2_20240403\riscv32-esp-elf-gdb\bin;" +
            "C:\Espressif\tools\xtensa-esp-elf\esp-13.2.0_20240530\xtensa-esp-elf\bin;" +
            "C:\Espressif\tools\esp-clang\16.0.1-fe4f10a809\esp-clang\bin;" +
            "C:\Espressif\tools\riscv32-esp-elf\esp-13.2.0_20240530\riscv32-esp-elf\bin;" +
            "C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin;" +
            "C:\Espressif\tools\cmake\3.24.0\bin;" +
            "C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20240318\openocd-esp32\bin;" +
            "C:\Espressif\tools\ninja\1.11.1;" +
            "C:\Espressif\tools\idf-exe\1.0.3;" +
            "C:\Espressif\tools\ccache\4.8\ccache-4.8-windows-x86_64;" +
            "C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64;" +
            "C:\Espressif\tools\qemu-xtensa\esp_develop_9.0.0_20240606\qemu\bin;" +
            "C:\Espressif\tools\qemu-riscv32\esp_develop_9.0.0_20240606\qemu\bin;" +
            "C:\Espressif\frameworks\esp-idf-v5.3.1\tools;" +
            $env:PATH

# Run the specified command
Invoke-Expression $Command