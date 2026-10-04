idf_component_register(SRCS "why2025_wifi.c" REQUIRES esp_driver_gpio esp_hosted esp_wifi_remote)

# Compile patched copies of four pinned upstream files, leaving the managed
# component untouched. Fail configuration if the expected upstream text changes.
idf_component_get_property(hosted_dir espressif__esp_hosted COMPONENT_DIR)
idf_component_get_property(hosted_lib espressif__esp_hosted COMPONENT_LIB)
set(patched_dir "${CMAKE_CURRENT_BINARY_DIR}/hosted_compat")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/patch_hosted.py"
    "${hosted_dir}" "${patched_dir}" RESULT_VARIABLE patch_result)
if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Could not apply WHY2025 ESP-Hosted compatibility fixes")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/patch_hosted.py")
get_target_property(hosted_sources ${hosted_lib} SOURCES)
foreach(source IN LISTS hosted_sources)
    get_filename_component(name "${source}" NAME)
    if(name MATCHES "^(esp_hosted_api|transport_drv|sdio_wrapper|sdio_drv)\\.c$")
        list(REMOVE_ITEM hosted_sources "${source}")
        list(APPEND hosted_sources "${patched_dir}/${name}")
        get_filename_component(source_dir "${source}" DIRECTORY)
        get_filename_component(source_dir "${source_dir}" ABSOLUTE BASE_DIR "${hosted_dir}")
        target_include_directories(${hosted_lib} PRIVATE "${source_dir}")
    endif()
endforeach()
set_property(TARGET ${hosted_lib} PROPERTY SOURCES "${hosted_sources}")
