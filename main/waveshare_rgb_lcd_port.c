/*
 * Display bring-up for the supported 800x480 & 1024x600 RGB boards.
 * Adapted from Waveshare's porting demo (CC0-1.0).
 */
#include "waveshare_rgb_lcd_port.h"
#if CONFIG_CANFLIGHT_BOARD_SUNTON_4827S043R
#include "driver/spi_master.h"
#include "esp_lcd_touch_xpt2046.h"
#endif

static const char *TAG = "lcd_port";

typedef struct {
    const char *name;
    int de, vsync, hsync, pclk;
    int data[16];                /* B0..B4, G0..G5, R0..R4 */
    int i2c_sda, i2c_scl;
    bool has_ch422g;             /* backlight + touch reset via CH422G */
    bool has_ch32v003;           /* 7B: helper MCU at 0x24 */
    int bl_gpio;
    int tp_rst_gpio;
    int hs_pulse, hs_bp, hs_fp, vs_pulse, vs_bp, vs_fp;
    int pclk_hz;
    bool tp_mirror;
    bool has_xpt2046;
    int tp_sclk, tp_mosi, tp_miso, tp_cs, tp_irq;
} board_cfg_t;

__attribute__((unused)) static const board_cfg_t k_waveshare = {
    .name = "Waveshare ESP32-S3-Touch-LCD (4.3/5/7)",
    .de = 5, .vsync = 3, .hsync = 46, .pclk = 7,
    .data = { 14, 38, 18, 17, 10,
              39, 0, 45, 48, 47, 21,
              1, 2, 42, 41, 40 },
    .i2c_sda = 8, .i2c_scl = 9,
    .has_ch422g = true,
    .bl_gpio = -1,
    .tp_rst_gpio = -1,
    .hs_pulse = 4, .hs_bp = 8, .hs_fp = 8,
    .vs_pulse = 4, .vs_bp = 8, .vs_fp = 8,
    .tp_mirror = false,
};

__attribute__((unused)) static const board_cfg_t k_waveshare_7b = {
    .name = "Waveshare ESP32-S3-Touch-LCD-7B (1024x600)",
    .de = 5, .vsync = 3, .hsync = 46, .pclk = 7,
    .data = { 14, 38, 18, 17, 10,
              39, 0, 45, 48, 47, 21,
              1, 2, 42, 41, 40 },
    .i2c_sda = 8, .i2c_scl = 9,
    .has_ch422g = false,
    .has_ch32v003 = true,
    .bl_gpio = -1,
    .tp_rst_gpio = -1,
    .hs_pulse = 162, .hs_bp = 152, .hs_fp = 48,
    .vs_pulse = 45, .vs_bp = 13, .vs_fp = 3,
    .tp_mirror = false,
};

__attribute__((unused)) static const board_cfg_t k_guition = {
    .name = "Guition JC8048W550",
    .de = 40, .vsync = 41, .hsync = 39, .pclk = 42,
    .data = { 8, 3, 46, 9, 1,
              5, 6, 7, 15, 16, 4,
              45, 48, 47, 21, 14 },
    .i2c_sda = 19, .i2c_scl = 20,
    .has_ch422g = false,
    .bl_gpio = 2,
    .tp_rst_gpio = 38,
    .hs_pulse = 4, .hs_bp = 8, .hs_fp = 8,
    .vs_pulse = 4, .vs_bp = 8, .vs_fp = 8,
    .tp_mirror = false,
};

__attribute__((unused)) static const board_cfg_t k_crowpanel_50 = {
    .name = "Elecrow CrowPanel 5.0 (4MB)",
    .de = 40, .vsync = 41, .hsync = 39, .pclk = 0,
    .data = { 8, 3, 46, 9, 1,
              5, 6, 7, 15, 16, 4,
              45, 48, 47, 21, 14 },
    .i2c_sda = 19, .i2c_scl = 20,
    .has_ch422g = false,
    .bl_gpio = 2,
    .tp_rst_gpio = -1,
    .hs_pulse = 4, .hs_bp = 8, .hs_fp = 8,
    .vs_pulse = 4, .vs_bp = 8, .vs_fp = 8,
    .tp_mirror = false,
};

__attribute__((unused)) static const board_cfg_t k_sunton_4827 = {
    .name = "Sunton ESP32-4827S043 (480x272)",
    .de = 40, .vsync = 41, .hsync = 39, .pclk = 42,
    .data = { 8, 3, 46, 9, 1,
              5, 6, 7, 15, 16, 4,
              45, 48, 47, 21, 14 },
    .i2c_sda = 19, .i2c_scl = 20,
    .has_ch422g = false,
    .bl_gpio = 2,
    .tp_rst_gpio = 38,
    .hs_pulse = 4, .hs_bp = 43, .hs_fp = 8,
    .vs_pulse = 4, .vs_bp = 12, .vs_fp = 8,
    .pclk_hz = 8 * 1000 * 1000,
    .tp_mirror = false,
};

__attribute__((unused)) static const board_cfg_t k_sunton_4827r = {
    .name = "Sunton ESP32-4827S043R (480x272, resistive)",
    .de = 40, .vsync = 41, .hsync = 39, .pclk = 42,
    .data = { 8, 3, 46, 9, 1,
              5, 6, 7, 15, 16, 4,
              45, 48, 47, 21, 14 },
    .i2c_sda = -1, .i2c_scl = -1,
    .has_ch422g = false,
    .bl_gpio = 2,
    .tp_rst_gpio = -1,
    .hs_pulse = 4, .hs_bp = 43, .hs_fp = 8,
    .vs_pulse = 4, .vs_bp = 12, .vs_fp = 8,
    .pclk_hz = 8 * 1000 * 1000,
    .tp_mirror = false,
    .has_xpt2046 = true,
    .tp_sclk = 12, .tp_mosi = 11, .tp_miso = 13, .tp_cs = 38, .tp_irq = 18,
};

static const board_cfg_t *s_board = &k_waveshare;
static esp_lcd_panel_handle_t s_panel;

const char *waveshare_lcd_board_name(void)
{
    return s_board->name;
}

void waveshare_lcd_get_res(int *w, int *h)
{
    *w = EXAMPLE_LCD_H_RES;
    *h = EXAMPLE_LCD_V_RES;
}

void *waveshare_lcd_get_fb(void)
{
    if (s_panel == NULL) {
        return NULL;
    }
    void *fb0 = NULL, *fb1 = NULL;
    if (esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &fb0, &fb1) != ESP_OK) {
        return NULL;
    }
    return fb0;
}

IRAM_ATTR static bool rgb_lcd_on_vsync_event(esp_lcd_panel_handle_t panel,
                                             const esp_lcd_rgb_panel_event_data_t *edata,
                                             void *user_ctx)
{
    return lvgl_port_notify_rgb_vsync();
}

/* Recover stuck I2C bus by pulsing SCL up to 9 times */
static void i2c_bus_recovery(int sda, int scl)
{
    gpio_config_t conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&conf);

    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(10);

    /* If SDA is held low by a slave, clock SCL until SDA goes high */
    for (int i = 0; i < 9 && gpio_get_level(sda) == 0; i++) {
        gpio_set_level(scl, 0);
        esp_rom_delay_us(10);
        gpio_set_level(scl, 1);
        esp_rom_delay_us(10);
    }

    /* Manual STOP condition */
    gpio_set_level(sda, 0);
    esp_rom_delay_us(10);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sda, 1);
    esp_rom_delay_us(10);
}

static esp_err_t i2c_master_init(int sda, int scl)
{
    i2c_bus_recovery(sda, scl);

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_MASTER_NUM, &i2c_conf);
    return i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0);
}

static void i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, addr, NULL, 0, pdMS_TO_TICKS(20));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  -> Found device at I2C address 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  -> No I2C devices responded on SDA/SCL pins!");
    }
}

static esp_err_t ch422g_write(uint8_t addr, uint8_t val)
{
    return i2c_master_write_to_device(I2C_MASTER_NUM, addr, &val, 1,
                                      I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static uint8_t s_ch32_out = 0xFF;

static esp_err_t ch32v003_reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, buf, 2,
                                      I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void ch32v003_output(uint8_t pin, uint8_t value)
{
    if (value) {
        s_ch32_out |= (1 << pin);
    } else {
        s_ch32_out &= ~(1 << pin);
    }
    ch32v003_reg_write(0x03, s_ch32_out);
}

static void ch32v003_backlight_pct(int pct)
{
    if (pct > 97) pct = 97;
    if (pct < 0) pct = 0;
    ch32v003_reg_write(0x05, (uint8_t)((100 - pct) * 255 / 100));
}

static void board_detect(void)
{
#if CONFIG_CANFLIGHT_BOARD_WAVESHARE_7
    s_board = &k_waveshare;
    i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
#elif CONFIG_CANFLIGHT_BOARD_GUITION_JC8048W550
    s_board = &k_guition;
    i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
#elif CONFIG_CANFLIGHT_BOARD_WAVESHARE_7B
    s_board = &k_waveshare_7b;
    i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
    esp_err_t err1 = ch32v003_reg_write(0x02, 0xFF);
    esp_err_t err2 = ch32v003_reg_write(0x03, s_ch32_out);
    ESP_LOGI(TAG, "CH32V003 init status: 0x02=%s, 0x03=%s",
             esp_err_to_name(err1), esp_err_to_name(err2));
#elif CONFIG_CANFLIGHT_BOARD_CROWPANEL_50
    s_board = &k_crowpanel_50;
    i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
#elif CONFIG_CANFLIGHT_BOARD_SUNTON_4827S043
    s_board = &k_sunton_4827;
    i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
#elif CONFIG_CANFLIGHT_BOARD_SUNTON_4827S043R
    s_board = &k_sunton_4827r;
#else
    i2c_master_init(k_waveshare.i2c_sda, k_waveshare.i2c_scl);
    if (ch422g_write(0x24, 0x01) == ESP_OK) {
        s_board = &k_waveshare;
    } else {
        s_board = &k_guition;
        i2c_driver_delete(I2C_MASTER_NUM);
        i2c_master_init(s_board->i2c_sda, s_board->i2c_scl);
    }
#endif
    ESP_LOGI(TAG, "board: %s", s_board->name);
    i2c_scan();
}

static void touch_reset(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 1ULL << GPIO_TOUCH_INT,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_TOUCH_INT, 0);  /* INT low during reset -> address 0x5D */

    if (s_board->has_ch32v003) {
        s_ch32_out &= ~((1 << 1) | (1 << 3));
        ch32v003_reg_write(0x03, s_ch32_out);
        vTaskDelay(pdMS_TO_TICKS(100));

        s_ch32_out |= ((1 << 1) | (1 << 3));
        ch32v003_reg_write(0x03, s_ch32_out);
        vTaskDelay(pdMS_TO_TICKS(20));

        gpio_set_direction(GPIO_TOUCH_INT, GPIO_MODE_INPUT);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else if (s_board->has_ch422g) {
        ch422g_write(0x24, 0x01);
        ch422g_write(0x38, 0x2C);
        vTaskDelay(pdMS_TO_TICKS(50));
        ch422g_write(0x38, 0x2E);
        vTaskDelay(pdMS_TO_TICKS(20));

        gpio_set_direction(GPIO_TOUCH_INT, GPIO_MODE_INPUT);
        vTaskDelay(pdMS_TO_TICKS(100));
    } else if (s_board->tp_rst_gpio >= 0) {
        gpio_config_t rst_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .pin_bit_mask = 1ULL << s_board->tp_rst_gpio,
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&rst_conf);
        gpio_set_level(s_board->tp_rst_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(s_board->tp_rst_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(20));

        gpio_set_direction(GPIO_TOUCH_INT, GPIO_MODE_INPUT);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Post-reset bus scan:");
    i2c_scan();
}

esp_err_t waveshare_esp32_s3_rgb_lcd_init(void)
{
    board_detect();

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = s_board->pclk_hz ? s_board->pclk_hz
                                        : EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            .hsync_pulse_width = s_board->hs_pulse,
            .hsync_back_porch = s_board->hs_bp,
            .hsync_front_porch = s_board->hs_fp,
            .vsync_pulse_width = s_board->vs_pulse,
            .vsync_back_porch = s_board->vs_bp,
            .vsync_front_porch = s_board->vs_fp,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width = EXAMPLE_RGB_DATA_WIDTH,
        .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL,
        .num_fbs = LVGL_PORT_LCD_RGB_BUFFER_NUMS,
        .bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = s_board->hsync,
        .vsync_gpio_num = s_board->vsync,
        .de_gpio_num = s_board->de,
        .pclk_gpio_num = s_board->pclk,
        .disp_gpio_num = -1,
        .flags = {
            .fb_in_psram = 1,
        },
    };
    for (int i = 0; i < 16; i++) {
        panel_config.data_gpio_nums[i] = s_board->data[i];
    }
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    s_panel = panel_handle;

    esp_lcd_touch_handle_t tp_handle = NULL;
#if CONFIG_CANFLIGHT_BOARD_SUNTON_4827S043R
    ESP_LOGI(TAG, "Initialize XPT2046 touch");
    spi_bus_config_t buscfg = {
        .sclk_io_num = s_board->tp_sclk,
        .mosi_io_num = s_board->tp_mosi,
        .miso_io_num = s_board->tp_miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_cfg =
        ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(s_board->tp_cs);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                              &tp_io_cfg, &tp_io));
    esp_lcd_touch_config_t xpt_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = s_board->tp_irq,
    };
    if (esp_lcd_touch_new_spi_xpt2046(tp_io, &xpt_cfg, &tp_handle) != ESP_OK) {
        ESP_LOGE(TAG, "XPT2046 init failed - running without touch");
        tp_handle = NULL;
    }
#else
    ESP_LOGI(TAG, "Initialize GT911 touch");
    touch_reset();

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 0;

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = GPIO_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = s_board->tp_mirror ? 1 : 0,
            .mirror_y = s_board->tp_mirror ? 1 : 0,
        },
    };

    esp_err_t terr = ESP_FAIL;
    uint8_t addresses[] = { 0x5D, 0x14, 0xBA, 0x28 };
    for (int i = 0; i < (sizeof(addresses) / sizeof(addresses[0])) && terr != ESP_OK; i++) {
        tp_io_config.dev_addr = addresses[i];
        if (esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM,
                                     &tp_io_config, &tp_io_handle) != ESP_OK) {
            continue;
        }
        terr = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle);
        if (terr != ESP_OK) {
            esp_lcd_panel_io_del(tp_io_handle);
            tp_io_handle = NULL;
        } else {
            ESP_LOGI(TAG, "GT911 touch successfully attached at address 0x%02X", addresses[i]);
        }
    }
    if (terr != ESP_OK) {
        ESP_LOGE(TAG, "GT911 init failed - running without touch");
        tp_handle = NULL;
    }
#endif

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
#if EXAMPLE_RGB_BOUNCE_BUFFER_SIZE > 0
        .on_bounce_frame_finish = rgb_lcd_on_vsync_event,
#else
        .on_vsync = rgb_lcd_on_vsync_event,
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));

    return ESP_OK;
}

static esp_err_t bl_gpio_set(int level)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 1ULL << s_board->bl_gpio,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    return gpio_set_level(s_board->bl_gpio, level);
}

esp_err_t waveshare_rgb_lcd_bl_on(void)
{
    if (s_board->has_ch32v003) {
        ch32v003_output(2, 1);
        ch32v003_backlight_pct(90);
        return ESP_OK;
    }
    if (!s_board->has_ch422g) {
        return bl_gpio_set(1);
    }
    ch422g_write(0x24, 0x01);
    return ch422g_write(0x38, 0x1E);
}

esp_err_t waveshare_rgb_lcd_bl_off(void)
{
    if (s_board->has_ch32v003) {
        ch32v003_backlight_pct(0);
        ch32v003_output(2, 0);
        return ESP_OK;
    }
    if (!s_board->has_ch422g) {
        return bl_gpio_set(0);
    }
    ch422g_write(0x24, 0x01);
    return ch422g_write(0x38, 0x1A);
}
