#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pins_config.h"
#include "sdkconfig.h"
#include "xl9535.h"
#include <stdio.h>

static const char *TAG = "T-RGB example";

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define I2C_MASTER_PORT             I2C_NUM_0
#define USING_2_1_INC_CST820        1
#define TOUCH_MODULES_CST_SELF

#include "TouchLib.h"


#define TOUCH_SLAVE_ADDRESS CST820_SLAVE_ADDRESS

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);

TouchLib touch;

typedef struct {
  uint8_t cmd;
  uint8_t data[16];
  uint8_t databytes; // No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

LV_IMG_DECLARE(round_pallette)
LV_IMG_DECLARE(gradient_true)
LV_IMG_DECLARE(gradient_24)
LV_IMG_DECLARE(gradient_dithered)
LV_IMG_DECLARE(color_wheel)
LV_IMG_DECLARE(img_color_circle)

DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x10}, 0x05},
    {0xC0, {0x3b, 0x00}, 0x02},
    {0xC1, {0x0b, 0x02}, 0x02},
    {0xC2, {0x07, 0x02}, 0x02},
    {0xCC, {0x10}, 0x01},
    {0xCD, {0x08}, 0x01}, // 用565时屏蔽    666打开
    {0xb0, {0x00, 0x11, 0x16, 0x0e, 0x11, 0x06, 0x05, 0x09, 0x08, 0x21, 0x06, 0x13, 0x10, 0x29, 0x31, 0x18}, 0x10},
    {0xb1, {0x00, 0x11, 0x16, 0x0e, 0x11, 0x07, 0x05, 0x09, 0x09, 0x21, 0x05, 0x13, 0x11, 0x2a, 0x31, 0x18}, 0x10},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x11}, 0x05},
    {0xb0, {0x6d}, 0x01},
    {0xb1, {0x37}, 0x01},
    {0xb2, {0x81}, 0x01},
    {0xb3, {0x80}, 0x01},
    {0xb5, {0x43}, 0x01},
    {0xb7, {0x85}, 0x01},
    {0xb8, {0x20}, 0x01},
    {0xc1, {0x78}, 0x01},
    {0xc2, {0x78}, 0x01},
    {0xc3, {0x8c}, 0x01},
    {0xd0, {0x88}, 0x01},
    {0xe0, {0x00, 0x00, 0x02}, 0x03},
    {0xe1, {0x03, 0xa0, 0x00, 0x00, 0x04, 0xa0, 0x00, 0x00, 0x00, 0x20, 0x20}, 0x0b},
    {0xe2, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x0d},
    {0xe3, {0x00, 0x00, 0x11, 0x00}, 0x04},
    {0xe4, {0x22, 0x00}, 0x02},
    {0xe5, {0x05, 0xec, 0xa0, 0xa0, 0x07, 0xee, 0xa0, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x10},
    {0xe6, {0x00, 0x00, 0x11, 0x00}, 0x04},
    {0xe7, {0x22, 0x00}, 0x02},
    {0xe8, {0x06, 0xed, 0xa0, 0xa0, 0x08, 0xef, 0xa0, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 0x10},
    {0xeb, {0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00}, 0x07},
    {0xed, {0xff, 0xff, 0xff, 0xba, 0x0a, 0xbf, 0x45, 0xff, 0xff, 0x54, 0xfb, 0xa0, 0xab, 0xff, 0xff, 0xff}, 0x10},
    {0xef, {0x10, 0x0d, 0x04, 0x08, 0x3f, 0x1f}, 0x06},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x13}, 0x05},
    {0xef, {0x08}, 0x01},
    {0xFF, {0x77, 0x01, 0x00, 0x00, 0x00}, 0x05},
    {0x36, {0x08}, 0x01},
    //{0x3a, {0x66}, 0x01}, //old
    {0x3a, {0x70}, 0x01}, // new according to COLMOD , we should send "x111 xxx"
    {0x11, {0x00}, 0x80},
    // {0xFF, {0x77, 0x01, 0x00, 0x00, 0x12}, 0x05},
    // {0xd1, {0x81}, 0x01},
    // {0xd2, {0x06}, 0x01},
    {0x29, {0x00}, 0x80},
    {0, {0}, 0xff}};

static void tft_init(void);

static int readRegister(uint8_t devAddr, uint16_t regAddr, uint8_t *data, uint8_t len) {
  uint8_t read_data = 0;
  i2c_master_write_read_device(I2C_MASTER_PORT, devAddr, (uint8_t *)&regAddr, 1, data, len, 1000 / portTICK_PERIOD_MS);
  return 0;
}

static int writeRegister(uint8_t devAddr, uint16_t regAddr, uint8_t *data, uint8_t len) {
  uint8_t write_buf[100] = {(uint8_t)regAddr};
  memcpy(write_buf + 1, data, len);
  i2c_master_write_to_device(I2C_MASTER_PORT, devAddr, write_buf, len + 1, 1000 / portTICK_PERIOD_MS);
  return 0;
}

static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data) {
  if (touch.read()) {
    printf("touch detected \n");
    TP_Point t = touch.getPoint(0);
    data->point.x = t.x;
    data->point.y = t.y;
    // get_img_color(t.x,t.y);

    const lv_color32_t *c32 = (lv_color32_t *)img_color_circle.data;
    c32 += t.y * img_color_circle.header.w + t.x;
    printf("red = %u \n", c32->red);
    printf("green = %u \n", c32->green);
    printf("blue = %u \n", c32->blue);
    // c32->red, c32->green, c32->blue

    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}


static bool example_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data,
                                   void *user_data) {
  BaseType_t high_task_awoken = pdFALSE;
  return high_task_awoken == pdTRUE;
}

static void disp_flush(lv_disp_t *disp_drv, const lv_area_t *area, lv_color_t *px_map) {
  printf("display flush \n");
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_disp_get_user_data(disp_drv);
  int offsetx1 = area->x1;
  int offsetx2 = area->x2;
  int offsety1 = area->y1;
  int offsety2 = area->y2;

  // pass the draw buffer to the driver
  esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
  lv_disp_flush_ready(disp_drv);
}

static void example_increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

extern "C" void app_main(void) {

  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = IIC_SDA_PIN,
      .scl_io_num = IIC_SCL_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master =
          {
              .clk_speed = 100 * 1000,
          },
  };
  esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &conf);

#if LV_COLOR_DEPTH == 24
  printf("24 bit color depth\n");
#endif

  if (err != ESP_OK) {
    ESP_LOGI(TAG, "i2c bus creation failed");
  }
  i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);

  ESP_LOGI(TAG, "Initialize xl9535");
  xl9535_begin(0, 0, 0, I2C_MASTER_PORT);
  xl9535_pinMode(PWR_EN_PIN, OUTPUT);
  xl9535_digitalWrite(PWR_EN_PIN, 1);

  ESP_LOGI(TAG, "Initialize touch");

  xl9535_pinMode(TP_RES_PIN, OUTPUT);
  xl9535_digitalWrite(TP_RES_PIN, 1);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  xl9535_digitalWrite(TP_RES_PIN, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  xl9535_digitalWrite(TP_RES_PIN, 1);

  touch.begin(TOUCH_SLAVE_ADDRESS, -1, readRegister, writeRegister);

  ESP_LOGI(TAG, "Initialize st7701s");
  tft_init();

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
  ESP_LOGI(TAG, "Turn off LCD backlight");
  ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)EXAMPLE_PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT));
#endif

  ESP_LOGI(TAG, "Install RGB LCD panel driver");
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_rgb_panel_config_t panel_config = {
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .timings =
        {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            // The following parameters should refer to LCD spec
            .hsync_pulse_width = 1,
            .hsync_back_porch = 30,
            .hsync_front_porch = 50,
            .vsync_pulse_width = 1,
            .vsync_back_porch = 30,
            .vsync_front_porch = 20,
            .flags =
                {
                    .pclk_active_neg = true,
                },
        },

    .data_width = 16, // RGB565 in parallel mode, thus 16bit in width

#if CONFIG_EXAMPLE_USE_BOUNCE_BUFFER
    .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,
#endif
    .psram_trans_align = 64,
    .hsync_gpio_num = EXAMPLE_PIN_NUM_HSYNC,
    .vsync_gpio_num = EXAMPLE_PIN_NUM_VSYNC,
    .de_gpio_num = EXAMPLE_PIN_NUM_DE,
    .pclk_gpio_num = EXAMPLE_PIN_NUM_PCLK,
    .disp_gpio_num = EXAMPLE_PIN_NUM_DISP_EN,
    .data_gpio_nums =
        {
            // EXAMPLE_PIN_NUM_DATA12, // 2.1
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
            EXAMPLE_PIN_NUM_DATA16,
            EXAMPLE_PIN_NUM_DATA17,

            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,

            // EXAMPLE_PIN_NUM_DATA0, // 2.1
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
        },
    .flags =
        {
            .fb_in_psram = true, // allocate frame buffer in PSRAM

        },
  };
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

  ESP_LOGI(TAG, "Register event callbacks");
  esp_lcd_rgb_panel_event_callbacks_t cbs = {
      .on_vsync = example_on_vsync_event,
  };

  // ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv));

  ESP_LOGI(TAG, "Initialize RGB LCD panel");
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
  ESP_LOGI(TAG, "Turn on LCD backlight");
  gpio_set_level((gpio_num_t)EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

  ESP_LOGI(TAG, "Initialize LVGL library");
  lv_init();
  lv_disp_t *disp = lv_disp_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
  void *buf1 = NULL;
  void *buf2 = NULL;
  buf1 = malloc((EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES) * sizeof(lv_color_t));
  assert(buf1);
  buf2 = malloc((EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES) * sizeof(lv_color_t));
  assert(buf2);

  lv_disp_set_draw_buffers(disp, buf1, buf2, (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES) * sizeof(lv_color_t),
                           LV_DISP_RENDER_MODE_FULL);
  lv_disp_set_flush_cb(disp, (lv_disp_flush_cb_t)(disp_flush));
  lv_disp_set_user_data(disp, panel_handle);

  // touch
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchpad_read);

  ESP_LOGI(TAG, "Install LVGL tick timer");
  const esp_timer_create_args_t lvgl_tick_timer_args = {.callback = &example_increase_lvgl_tick, .name = "lvgl_tick"};
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

  lv_obj_t *meter = lv_meter_create(lv_scr_act());

  /*Remove the background and the circle from the middle*/
  lv_obj_remove_style(meter, NULL, LV_PART_MAIN);
  lv_obj_remove_style(meter, NULL, LV_PART_INDICATOR);

  lv_obj_set_size(meter, 480, 480);
  lv_obj_center(meter);

  /*Add a scale first with no ticks.*/
  lv_meter_set_scale_ticks(meter, 0, 0, 0, lv_color_black());
  lv_meter_set_scale_range(meter, 0, 100, 360, 0);

  /*Add a three arc indicator*/
  lv_coord_t indic_w = 200;


  lv_color_t test_color[20];
  lv_meter_indicator_t *indic[20];
  for (int i = 0; i < 20; i++) {
    test_color[i] = lv_color_make(95, 124 + i, 95);
    indic[i] = lv_meter_add_arc(meter, indic_w, test_color[i], 0);
    lv_meter_set_indicator_start_value(meter, indic[i], 0+(5*i));
    lv_meter_set_indicator_end_value(meter, indic[i], 5+(5*i));
  }



  while (1) {
    uint32_t task_delay_ms = lv_timer_handler();
    if (task_delay_ms > 500) {
      task_delay_ms = 500;
    } else if (task_delay_ms < 5) {
      task_delay_ms = 5;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

static void lcd_send_data(uint8_t data) {
  uint8_t n;
  for (n = 0; n < 8; n++) {
    if (data & 0x80)
      xl9535_digitalWrite(LCD_SDA_PIN, 1);
    else
      xl9535_digitalWrite(LCD_SDA_PIN, 0);

    data <<= 1;
    xl9535_digitalWrite(LCD_CLK_PIN, 0);
    xl9535_digitalWrite(LCD_CLK_PIN, 1);
  }
}

static void lcd_cmd(const uint8_t cmd) {
  xl9535_digitalWrite(LCD_CS_PIN, 0);
  xl9535_digitalWrite(LCD_SDA_PIN, 0);
  xl9535_digitalWrite(LCD_CLK_PIN, 0);
  xl9535_digitalWrite(LCD_CLK_PIN, 1);
  lcd_send_data(cmd);
  xl9535_digitalWrite(LCD_CS_PIN, 1);
}

static void lcd_data(const uint8_t *data, int len) {
  uint32_t i = 0;
  if (len == 0)
    return; // no need to send anything
  do {
    xl9535_digitalWrite(LCD_CS_PIN, 0);
    xl9535_digitalWrite(LCD_SDA_PIN, 1);
    xl9535_digitalWrite(LCD_CLK_PIN, 0);
    xl9535_digitalWrite(LCD_CLK_PIN, 1);
    lcd_send_data(*(data + i));
    xl9535_digitalWrite(LCD_CS_PIN, 1);
    i++;
  } while (len--);
}

static void tft_init(void) {
  int cmd = 0;
  xl9535_pinMode(LCD_SDA_PIN, OUTPUT);
  xl9535_pinMode(LCD_CLK_PIN, OUTPUT);
  xl9535_pinMode(LCD_CS_PIN, OUTPUT);
  xl9535_pinMode(LCD_RST_PIN, OUTPUT);
  // xl9535_read_all_reg();

  xl9535_digitalWrite(LCD_SDA_PIN, 1);
  xl9535_digitalWrite(LCD_CLK_PIN, 1);
  xl9535_digitalWrite(LCD_CS_PIN, 1);

  // Reset the display
  xl9535_digitalWrite(LCD_RST_PIN, 1);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  xl9535_digitalWrite(LCD_RST_PIN, 0);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  xl9535_digitalWrite(LCD_RST_PIN, 1);
  vTaskDelay(200 / portTICK_PERIOD_MS);

  while (st_init_cmds[cmd].databytes != 0xff) {
    lcd_cmd(st_init_cmds[cmd].cmd);
    lcd_data(st_init_cmds[cmd].data, st_init_cmds[cmd].databytes & 0x1F);
    if (st_init_cmds[cmd].databytes & 0x80) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    cmd++;
  }
}

