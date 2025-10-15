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
        "screens/notification_screen.c"
        "screens/sensor/sensor_detail_screen.c"
        "screens/sensor/sensor_settings_screen.c"
        "screens/system/system_menu_screen.c"
        "screens/system/system_screens.c"
        
        # Pump screens
        "screens/pumps/pumps_menu_screen.c"
        "screens/pumps/pumps_status_screen.c"
        "screens/pumps/pumps_manual_screen.c"
        "screens/pumps/pump_calibration_screen.c"
        
        # Intelligent Adaptive PID screens (LEGACY PID удалён)
        "screens/adaptive/pid_intelligent_dashboard.c"
        "screens/adaptive/pid_intelligent_detail.c"
        "screens/adaptive/pid_auto_tune_screen.c"
        
        # Widgets
        "widgets/back_button.c"
        "widgets/status_bar.c"
        "widgets/menu_list.c"
        "widgets/sensor_card.c"
        "widgets/encoder_value_edit.c"
        "widgets/intelligent_pid_card.c"
        
    INCLUDE_DIRS 
        "."
        "screen_manager"
        "screens"
        "screens/base"
        "screens/sensor"
        "screens/system"
        "screens/pumps"
        "screens/adaptive"
        "widgets"
        
    REQUIRES
        pump_manager
        config_manager
        data_logger
        adaptive_pid
        pid_auto_tuner
        
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

