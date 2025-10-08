idf_component_register(
    SRCS 
        # Legacy files
        "lvgl_ui.c"
        "ui_manager.c"
        "sensor_screens_optimized.c"
        "ph_screen.c"
        
        # Screen Manager Core
        "screen_manager/screen_registry.c"
        "screen_manager/screen_lifecycle.c"
        "screen_manager/screen_navigator.c"
        "screen_manager/screen_manager.c"
        "screen_manager/screen_init.c"
        
        # Base screens & templates
        "screens/base/screen_base.c"
        "screens/base/screen_template.c"
        
        # Concrete screens
        "screens/main_screen.c"
        "screens/sensor/sensor_detail_screen.c"
        "screens/sensor/sensor_settings_screen.c"
        "screens/system/system_menu_screen.c"
        "screens/system/system_screens.c"
        
        # Widgets
        "widgets/back_button.c"
        "widgets/status_bar.c"
        "widgets/menu_list.c"
        "widgets/sensor_card.c"
        
    INCLUDE_DIRS 
        "."
        "screen_manager"
        "screens"
        "screens/base"
        "screens/sensor"
        "screens/system"
        "widgets"
        
    PRIV_REQUIRES 
        lvgl__lvgl 
        lcd_ili9341 
        encoder 
        main 
        ph_ec_controller
        nvs_flash
        config_manager
        data_logger
)

