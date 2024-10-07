#!/bin/bash
clear

# Check if the argument is provided
if [ -z "$1" ]; then
  echo "Error: No argument supplied. Simply provide a name of source i.e sha1"
  echo "Additionaly ./build sha1 clean"
  exit 1
fi

# Project name with the argument appended
SOURCE_FILE_NAME="$1"

# Clean-up (optional)
if [ "$2" == "clean" ]; then
    echo "Cleaning up..."
    rm -f ${SOURCE_FILE_NAME}.bin ${SOURCE_FILE_NAME}.elf ${SOURCE_FILE_NAME}-objdump.txt ${SOURCE_FILE_NAME}-readelf.txt ${SOURCE_FILE_NAME}.hex
    rm -rf build
    exit
fi

# Compile mainX.elf with additional linker script gridshell.ld
echo "Compiling to ${SOURCE_FILE_NAME}.elf..." 
~/.arduino15/packages/m5stack/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-gcc \
    -Wall -fno-common -mlongcalls -fdata-sections -ffunction-sections -Wl,-r -nostartfiles -nodefaultlibs -nostdlib -o ${SOURCE_FILE_NAME}.elf \
    -Wl,-Tesp32.ld ${SOURCE_FILE_NAME}.c \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/mbedtls/mbedtls/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/json/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/esp_rom/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/spiffs/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/libraries/SPIFFS/src/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/libraries/FS/src/ \
    -I ~/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/include/ \
    -I ~/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/xtensa-esp32-elf/sys-include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/esp_common/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/log/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/qout_qspi/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/soc/esp32/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.1.0/tools/sdk/esp32/include/vfs/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.1.0/tools/sdk/esp32/include/freertos/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/freertos/include/esp_additions/freertos \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/freertos/port/xtensa/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/xtensa/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/xtensa/esp32/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/esp_hw_support/include \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/hal/include \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/hal/esp32/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/soc/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.1.0/tools/sdk/esp32/include/esp_system/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/esp_timer/include \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/newlib/platform_include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/heap/include/ \
    -I ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/include/spiffs/include/ \
    -L ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/lib/ \
    -lm
  
    
# Can link against libs 
#   -L ~/.arduino15/packages/esp32/hardware/esp32/2.0.10/tools/sdk/esp32/lib/ \
#   -lmbedtls -lmbedcrypto -lc ...

# Strip unnecessary sections
echo "Stripping unneeded sections from ${SOURCE_FILE_NAME}.elf..."
~/.arduino15/packages/m5stack/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-strip --strip-unneeded ${SOURCE_FILE_NAME}.elf

# Generate objdump output
echo "Generating objdump..."
~/.arduino15/packages/m5stack/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-objdump -d -S -s -t -x -r ${SOURCE_FILE_NAME}.elf > ${SOURCE_FILE_NAME}-objdump.txt

# Generate readelf output
echo "Generating readelf..."
~/.arduino15/packages/m5stack/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-readelf -a ${SOURCE_FILE_NAME}.elf > ${SOURCE_FILE_NAME}-readelf.txt

# Generate hex
echo "Converting ${SOURCE_FILE_NAME}.elf to a C++ hex array..."
xxd -i "${SOURCE_FILE_NAME}.elf" > "${SOURCE_FILE_NAME}.hex"

# Replace the generated variable name with 'main_elf'
sed -i "s/${SOURCE_FILE_NAME}_elf/main_elf/g" "${SOURCE_FILE_NAME}.hex"

# Base64 encode the ELF file for web transfer
echo "Encoding ${SOURCE_FILE_NAME}.elf to base64..."
base64 "${SOURCE_FILE_NAME}.elf" > "${SOURCE_FILE_NAME}.bin"
 
echo "Build complete!"
echo "Deploy task to GIT ${SOURCE_FILE_NAME}.bin"
echo "Test the task with runner ${SOURCE_FILE_NAME}.hex"
echo "Send to tesselator ${SOURCE_FILE_NAME}.elf"

ls -lha ${SOURCE_FILE_NAME}.*
