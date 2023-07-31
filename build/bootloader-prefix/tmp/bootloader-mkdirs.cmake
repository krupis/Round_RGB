# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Lukas/esp/esp-idf/components/bootloader/subproject"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/tmp"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/src"
  "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Programming/ESP32/RGB_Controller_Round/T-RGB/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
