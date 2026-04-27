# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "E:/Software/ESP_IDF/Espressif/frameworks/esp-idf-v5.3.1/components/bootloader/subproject"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/tmp"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/src/bootloader-stamp"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/src"
  "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Workspace/Github_Space/HomeDevice/ESP32/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
