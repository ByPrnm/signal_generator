# Tentukan versi minimum CMake yang dibutuhkan
cmake_minimum_required(VERSION 3.13)

# Inisialisasi Pico SDK. Path ke SDK harus sudah diatur di environment
# Anda, atau Anda dapat menentukannya secara manual.
# Contoh: set(PICO_SDK_PATH "/path/to/pico-sdk")
include(pico_sdk_import.cmake)

# Tentukan nama proyek
project(pio_signal_generator C CXX ASM)

# Inisialisasi Pico SDK untuk proyek ini
pico_sdk_init()

# --- Tambahkan sumber kode dan target ---

# 1. Buat target executable terlebih dahulu
#    Ini memberitahu CMake bahwa ada sebuah program bernama "pio_signal_generator"
#    yang dibangun dari file main.c.
add_executable(signal_generator
    main.c
)

# 2. SEKARANG, proses file .pio dan tautkan hasilnya ke target yang sudah ada
#    Fungsi ini akan membuat file header .pio.h dan secara otomatis
#    menambahkannya sebagai dependency ke target "pio_signal_generator".
pico_generate_pio_header(signal_generator ${CMAKE_CURRENT_LIST_DIR}/signal_generator.pio)


# --- Tautkan (Link) Library yang Dibutuhkan ---

# Tautkan library standar Pico dan library hardware yang relevan
# - pico_stdlib: Fungsi dasar (stdio, gpio, etc.)
# - hardware_pio: Fungsi untuk berinteraksi dengan PIO
target_link_libraries(signal_generator PRIVATE
    pico_stdlib
    hardware_pio
    hardware_clocks 
    hardware_i2c
)

# --- Buat Output Tambahan ---

# Buat file .uf2 yang dapat di-drag-and-drop ke Pico
pico_add_extra_outputs(signal_generator)

# Aktifkan output printf melalui USB atau UART
pico_enable_stdio_usb(signal_generator 1)
pico_enable_stdio_uart(signal_generator 0)