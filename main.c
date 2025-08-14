#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "signal_generator.pio.h" // Header yang di-generate otomatis

// -- Konfigurasi Sinyal --
const uint PIN_CH1_BASE = 6;
const float FREQUENCY_HZ = 1000.0f;
const float PULSE_WIDTH_US = 5.0f;
const float PHASE_SHIFT_US = 5.0f;

// -- Deklarasi Fungsi --
void init_pio(PIO pio, uint *sm, uint *offset, float clk_div);
void calculate_delays(float sys_clk_hz, float pio_clk_div,
                      uint32_t *delay_A, uint32_t *delay_B,
                      uint32_t *delay_C, uint32_t *delay_D);

int main()
{
    stdio_init_all();

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

    // Aktifkan State Machine PIO
    pio_sm_set_enabled(pio, sm, true);

    // Loop utama untuk memberi data delay ke PIO secara kontinyu
    while (true)
    {
        pio_sm_put_blocking(pio, sm, delay_A);
        pio_sm_put_blocking(pio, sm, delay_B);
        pio_sm_put_blocking(pio, sm, delay_C);
        pio_sm_put_blocking(pio, sm, delay_D);
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

    // Frekuensi state machine PIO = frekuensi sistem / divider
    float pio_freq_hz = sys_clk_hz / pio_clk_div;

    // Total periode dalam mikrodetik
    float period_us = 1e6f / FREQUENCY_HZ;
    // Durasi event D (sisa waktu) dalam mikrodetik
    float duration_D_us = period_us - PULSE_WIDTH_US - PHASE_SHIFT_US - PULSE_WIDTH_US;

    // Konversi durasi (us) ke jumlah siklus PIO
    // Rumus: cycles = duration_us * (pio_freq_hz / 1e6)
    uint32_t cycles_A = (uint32_t)(PULSE_WIDTH_US * (pio_freq_hz / 1e6f));
    uint32_t cycles_B = (uint32_t)(PHASE_SHIFT_US * (pio_freq_hz / 1e6f));
    uint32_t cycles_C = (uint32_t)(PULSE_WIDTH_US * (pio_freq_hz / 1e6f));
    uint32_t cycles_D = (uint32_t)(duration_D_us * (pio_freq_hz / 1e6f));

    // PERBAIKAN: Koreksi nilai delay untuk loop di PIO.
    // Total siklus per event = pull(1) + mov(1) + set(1) + jmp_loop(N+1) = N + 4 siklus.
    // Jadi, nilai N (loop counter) yang dikirim ke PIO harus (Total Siklus - 4).
    *delay_A = cycles_A > 4 ? cycles_A - 4 : 0;
    *delay_B = cycles_B > 4 ? cycles_B - 4 : 0;
    *delay_C = cycles_C > 4 ? cycles_C - 4 : 0;
    *delay_D = cycles_D > 4 ? cycles_D - 4 : 0;
}

/**
 * @brief Menginisialisasi PIO, State Machine, dan GPIO yang relevan.
 * * @param pio Instance PIO (pio0 atau pio1)
 * @param sm Pointer untuk menyimpan nomor SM yang dialokasikan
 * @param offset Pointer untuk menyimpan offset program di instruction memory PIO
 * @param clk_div Nilai clock divider untuk SM
 */
void init_pio(PIO pio, uint *sm, uint *offset, float clk_div)
{
    // 1. Muat program PIO ke instruction memory dan dapatkan offsetnya
    *offset = pio_add_program(pio, &signal_generator_program);

    // 2. Klaim State Machine (SM) yang belum terpakai pada PIO
    *sm = pio_claim_unused_sm(pio, true);

    // 3. Dapatkan konfigurasi default untuk SM
    pio_sm_config c = signal_generator_program_get_default_config(*offset);

    // 4. Konfigurasi pin untuk instruksi 'set'
    // Basis pin di GP6, dan akan mengontrol 4 pin (GP6, GP7, GP8, GP9)
    sm_config_set_set_pins(&c, PIN_CH1_BASE, 4);

    // 5. Konfigurasi clock divider untuk SM
    sm_config_set_clkdiv(&c, clk_div);

    // 6. Inisialisasi GPIO untuk dikontrol oleh PIO
    for (uint i = 0; i < 4; ++i)
    {
        pio_gpio_init(pio, PIN_CH1_BASE + i);
    }

    // 7. Atur arah pin (semua sebagai output) untuk SM
    pio_sm_set_consecutive_pindirs(pio, *sm, PIN_CH1_BASE, 4, true);

    // 8. Muat konfigurasi ke SM
    pio_sm_init(pio, *sm, *offset, &c);
}
