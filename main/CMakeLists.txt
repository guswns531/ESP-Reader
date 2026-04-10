idf_component_register(
    SRCS
        "app_controller.cpp"
        "app_main.cpp"
        "display_manager.cpp"
        "document_parser.cpp"
        "epaper_hal.cpp"
        "input_manager.cpp"
        "storage_manager.cpp"
        "U8g2_for_Adafruit_GFX.cpp"
        "u8g2_font_unifont_t_korean2.cpp"
    PRIV_REQUIRES
        spiffs
        spi_flash
        esp_driver_spi
        esp_driver_gpio
        esp_adc
    INCLUDE_DIRS
        "."
    PRIV_INCLUDE_DIRS
        "../../third_party/adafruit_gfx"
        "../../third_party/adafruit_gfx/Fonts"
)

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
