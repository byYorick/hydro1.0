#include "lvgl_encoder.h"
#include "lvgl.h"
#include "../encoder/encoder.h"
#include "esp_log.h"

static const char *TAG = "LVGL_ENCODER";

// Encoder input device data structure
typedef struct {
    int32_t enc_diff;
    lv_indev_state_t state;
} encoder_indev_data_t;

static encoder_indev_data_t enc_data = {0};
static lv_indev_t *enc_indev = NULL;
static lv_group_t *encoder_group = NULL;

// LVGL encoder read callback function
static void encoder_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int32_t last_count = 0;
    
    // Get current encoder count
    int32_t current_count = encoder_get_count();
    
    // Calculate difference since last read
    enc_data.enc_diff = current_count - last_count;
    last_count = current_count;
    
    // Get button state
    enc_data.state = encoder_button_pressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    
    // Pass data to LVGL
    data->enc_diff = enc_data.enc_diff;
    data->state = enc_data.state;
    
    // Clear count if there was movement to prevent accumulation
    if (enc_data.enc_diff != 0) {
        encoder_clear_count();
        //ESP_LOGD(TAG, "Encoder diff: %ld, Button: %d", (long)enc_data.enc_diff, enc_data.state);
    }
}

// Initialize LVGL encoder integration
void lvgl_encoder_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL encoder integration");
    
    // Initialize encoder with configuration (if not already done)
    // Note: In your app_main.c, encoder is already initialized, so we just use it
    
    // Initialize LVGL input device driver for encoder
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read_cb;
    indev_drv.user_data = &enc_data;
    
    // Register the input device
    enc_indev = lv_indev_drv_register(&indev_drv);
    if (enc_indev == NULL) {
        ESP_LOGE(TAG, "Failed to register encoder input device");
        return;
    }
    
    // Create a group for encoder navigation
    encoder_group = lv_group_create();
    if (encoder_group == NULL) {
        ESP_LOGE(TAG, "Failed to create encoder group");
        return;
    }
    
    // Set the group for the encoder input device
    lv_indev_set_group(enc_indev, encoder_group);
    
    // Set as default group so objects are automatically added
    lv_group_set_default(encoder_group);
    
    // Enable group wrapping (cycling through objects)
    lv_group_set_wrap(encoder_group, true);
    
    ESP_LOGI(TAG, "LVGL encoder integration initialized successfully");
}

// Get the encoder input device
lv_indev_t* lvgl_encoder_get_indev(void)
{
    return enc_indev;
}

// Get the encoder group
lv_group_t* lvgl_encoder_get_group(void)
{
    return encoder_group;
}

// Add an object to the encoder group
void lvgl_encoder_add_obj(lv_obj_t *obj)
{
    if (encoder_group && obj) {
        lv_group_add_obj(encoder_group, obj);
    }
}