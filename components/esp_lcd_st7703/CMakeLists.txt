# Vendored from BadgeVMS (Espressif esp_lcd_st7703 1.0.3).
# Keyed on IDF_TARGET because RG_BUILD_TARGET isn't defined during idf's early requirements expansion.
if(IDF_TARGET STREQUAL "esp32p4")
    idf_component_register(SRCS "esp_lcd_st7703.c" INCLUDE_DIRS "include"
                           PRIV_REQUIRES driver REQUIRES esp_lcd)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        ESP_LCD_ST7703_VER_MAJOR=1 ESP_LCD_ST7703_VER_MINOR=0 ESP_LCD_ST7703_VER_PATCH=3)
else()
    idf_component_register()
endif()
