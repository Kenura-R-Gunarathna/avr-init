# Minimal bare-metal AVR Makefile
MCU      = atmega328p
F_CPU    = 1000000UL

# Target name
# Standard approach: TARGET = $(notdir $(CURDIR))
# Note: $(notdir $(CURDIR)) may fail if parent folders contain spaces.
TARGET   = $(notdir $(CURDIR))

# Directories
SRC_DIR   = src
BUILD_DIR = build

# Source and Object files
SRCS      = $(shell find $(SRC_DIR) -name "*.c")
OBJS      = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Fuses
LFUSE    = 0x62
HFUSE    = 0xD9
EFUSE    = 0xFF

CC       = avr-gcc
OBJCOPY  = avr-objcopy
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c11 \
           -ffunction-sections -fdata-sections -I$(SRC_DIR)
LDFLAGS  = -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

all: $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	avr-size --mcu=$(MCU) -C $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

flash: $(BUILD_DIR)/$(TARGET).hex
	avrdude -c arduino -p $(MCU) -P /dev/ttyUSB0 -b 115200 -U flash:w:$<

clean:
	rm -rf $(BUILD_DIR)
