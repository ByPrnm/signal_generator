/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "signal_generator.pio.h" // Header yang di-generate otomatis

// -- Konfigurasi Sinyal --
const uint PIN_CH1_BASE = 6;
const float FREQUENCY_HZ = 1000.0f;
const float PULSE_WIDTH_US = 5.0f;
const float PHASE_SHIFT_US = 5.0f;

// -- Konfigurasi Tombol --
const uint BUTTON_PIN = 13;
const uint64_t SIGNAL_DURATION_US = 5 * 1000 * 1000; // 5 detik

// -- Deklarasi Fungsi --
void init_pio(PIO pio, uint *sm, uint *offset, float clk_div);
void calculate_delays(float sys_clk_hz, float pio_clk_div,
                      uint32_t *delay_A, uint32_t *delay_B,
                      uint32_t *delay_C, uint32_t *delay_D);

int main()
{
    stdio_init_all();

    // -- Inisialisasi Tombol --
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); // Tombol terhubung ke ground, jadi butuh pull-up

    // -- Inisialisasi PIO --
    PIO pio = pio0;
    uint sm, offset;
    // Tentukan clock divider untuk PIO agar 1 siklus = 0.1 us
    // Ini memberikan resolusi yang baik dan menjaga nilai delay dalam rentang wajar
    float pio_clk_div = 12.5f;
    init_pio(pio, &sm, &offset, pio_clk_div);

    // -- Kalkulasi Durasi Delay --
    uint32_t delay_A, delay_B, delay_C, delay_D;
    calculate_delays(clock_get_hz(clk_sys), pio_clk_div, &delay_A, &delay_B, &delay_C, &delay_D);

    // Loop utama untuk menunggu penekanan tombol
    while (true)
    {
        // Tunggu tombol ditekan (pin menjadi LOW)
        if (!gpio_get(BUTTON_PIN))
        {

            // Aktifkan State Machine PIO untuk memulai pembangkitan sinyal
            pio_sm_set_enabled(pio, sm, true);

            // Catat waktu mulai
            absolute_time_t start_time = get_absolute_time();

            // Loop untuk memberi data delay ke PIO selama 5 detik
            while (absolute_time_diff_us(start_time, get_absolute_time()) < SIGNAL_DURATION_US)
            {
                pio_sm_put_blocking(pio, sm, delay_A);
                pio_sm_put_blocking(pio, sm, delay_B);
                pio_sm_put_blocking(pio, sm, delay_C);
                pio_sm_put_blocking(pio, sm, delay_D);
            }

            // Nonaktifkan State Machine PIO untuk menghentikan sinyal
            pio_sm_set_enabled(pio, sm, false);

            // Tunggu hingga tombol dilepas untuk menghindari pemicuan berulang
            while (!gpio_get(BUTTON_PIN))
            {
                tight_loop_contents();
            }
        }
        // Jika tombol tidak ditekan, Pico tidak melakukan apa-apa
        tight_loop_contents();
    }
}

/**
 * @brief Menghitung nilai delay untuk setiap event dalam satuan siklus PIO.
 * * @param sys_clk_hz Frekuensi clock sistem (Hz)
 * @param pio_clk_div Clock divider yang dikonfigurasi untuk PIO SM
 * @param delay_A Pointer untuk menyimpan delay event A
 * @param delay_B Pointer untuk menyimpan delay event B
 * @param delay_C Pointer untuk menyimpan delay event C
 * @param delay_D Pointer untuk menyimpan delay event D
 */
void calculate_delays(float sys_clk_hz, float pio_clk_div,
                      uint32_t *delay_A, uint32_t *delay_B,
                      uint32_t *delay_C, uint32_t *delay_D)
{
    float pio_clk_hz = sys_clk_hz / pio_clk_div;
    float period_s = 1.0f / FREQUENCY_HZ;
    uint32_t total_pio_cycles = (uint32_t)(period_s * pio_clk_hz);
    uint32_t pulse_width_cycles = (uint32_t)(PULSE_WIDTH_US * 1e-6f * pio_clk_hz);
    uint32_t phase_shift_cycles = (uint32_t)(PHASE_SHIFT_US * 1e-6f * pio_clk_hz);

    // Durasi setiap event dalam siklus PIO
    uint32_t event_A_duration = pulse_width_cycles;
    uint32_t event_B_duration = phase_shift_cycles;
    uint32_t event_C_duration = pulse_width_cycles;
    uint32_t event_D_duration = total_pio_cycles - event_A_duration - event_B_duration - event_C_duration;

    // Nilai N (loop counter) yang dikirim ke PIO
    // Rumus: N = durasi_siklus - overhead_instruksi
    // Overhead untuk program PIO ini adalah 4 siklus per loop
    *delay_A = event_A_duration > 4 ? event_A_duration - 4 : 0;
    *delay_B = event_B_duration > 4 ? event_B_duration - 4 : 0;
    *delay_C = event_C_duration > 4 ? event_C_duration - 4 : 0;
    *delay_D = event_D_duration > 4 ? event_D_duration - 4 : 0;
}

/**
 * @brief Menginisialisasi PIO, memuat program, dan mengkonfigurasi state machine.
 *
 * @param pio Instance PIO yang akan digunakan (pio0 atau pio1)
 * @param sm Pointer untuk menyimpan nomor state machine yang dialokasikan
 * @param offset Pointer untuk menyimpan offset program PIO di instruction memory
 * @param clk_div Nilai clock divider untuk state machine
 */
void init_pio(PIO pio, uint *sm, uint *offset, float clk_div)
{
    *offset = pio_add_program(pio, &signal_generator_program);
    *sm = pio_claim_unused_sm(pio, true);
    pio_sm_config c = signal_generator_program_get_default_config(*offset);

    // Konfigurasi pin-pin yang akan digunakan oleh PIO
    // Pin dasar untuk 'set' adalah PIN_CH1_BASE, dan akan mempengaruhi 4 pin secara berurutan
    sm_config_set_set_pins(&c, PIN_CH1_BASE, 4);
    for (uint i = 0; i < 4; ++i)
    {
        pio_gpio_init(pio, PIN_CH1_BASE + i);
    }
    pio_sm_set_consecutive_pindirs(pio, *sm, PIN_CH1_BASE, 4, true);

    // Atur clock divider
    sm_config_set_clkdiv(&c, clk_div);

    // Terapkan konfigurasi ke state machine
    pio_sm_init(pio, *sm, *offset, &c);
}
