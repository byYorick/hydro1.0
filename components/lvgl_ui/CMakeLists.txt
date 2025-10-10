idf_component_register(
    SRCS 
        # Core UI
        "lvgl_ui.c"
        # LEGACY УДАЛЕНО: ui_manager.c, sensor_screens_optimized.c, ph_screen.c
        
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
        "screens/popup_screen.c"
        "screens/sensor/sensor_detail_screen.c"
        "screens/sensor/sensor_settings_screen.c"
        "screens/system/system_menu_screen.c"
        "screens/system/system_screens.c"
        
        # Pump screens
        "screens/pumps/pumps_status_screen.c"
        "screens/pumps/pumps_manual_screen.c"
        "screens/pumps/pump_calibration_screen.c"
        
        # PID screens
        "screens/pid/pid_main_screen.c"
        "screens/pid/pid_detail_screen.c"
        "screens/pid/pid_tuning_screen.c"
        "screens/pid/pid_advanced_screen.c"
        "screens/pid/pid_thresholds_screen.c"
        "screens/pid/pid_graph_screen.c"
        
        # Widgets
        "widgets/back_button.c"
        "widgets/status_bar.c"
        "widgets/menu_list.c"
        "widgets/sensor_card.c"
        "widgets/notification_popup.c"
        
    INCLUDE_DIRS 
        "."
        "screen_manager"
        "screens"
        "screens/base"
        "screens/sensor"
        "screens/system"
        "screens/pumps"
        "screens/pid"
        "widgets"
        
    REQUIRES
        pump_manager
        config_manager
        data_logger
        
    PRIV_REQUIRES 
        lvgl__lvgl 
        lcd_ili9341 
        encoder 
        main 
        ph_ec_controller
        nvs_flash
        notification_system
        error_handler
)

