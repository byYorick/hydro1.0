#pragma once

#include "lvgl.h"

/**
 * @brief Initialize LVGL encoder integration
 */
void lvgl_encoder_init(void);

/**
 * @brief Get the encoder input device
 * 
 * @return lv_indev_t* Pointer to the encoder input device
 */
lv_indev_t* lvgl_encoder_get_indev(void);

/**
 * @brief Get the encoder group
 * 
 * @return lv_group_t* Pointer to the encoder group
 */
lv_group_t* lvgl_encoder_get_group(void);

/**
 * @brief Add an object to the encoder group
 * 
 * @param obj Pointer to the object to add
 */
void lvgl_encoder_add_obj(lv_obj_t *obj);