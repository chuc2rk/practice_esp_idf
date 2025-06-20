# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v4.4.7/components/bootloader/subproject"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix/tmp"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix/src"
  "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Github/practice_esp_idf/mb_master_control_relay_module_esp_modbus/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
