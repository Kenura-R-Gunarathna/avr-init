
# Minimal bare-metal AVR Makefile
MCU      = atmega328p
F_CPU    = 16000000UL
TARGET   = blink
SRC      = $(wildcard *.c)

CC       = avr-gcc
OBJCOPY  = avr-objcopy
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c11 \
           -ffunction-sections -fdata-sections
LDFLAGS  = -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map=$(TARGET).map

all: $(TARGET).hex

$(TARGET).elf: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	avr-size --mcu=$(MCU) -C $<

flash: $(TARGET).hex
	avrdude -c arduino -p $(MCU) -P /dev/ttyUSB0 -b 115200 -U flash:w:$<

clean:
	rm -f *.elf *.hex *.map *.o
