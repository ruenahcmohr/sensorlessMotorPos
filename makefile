

SRC = $(wildcard *.c)
ASRC = $(wildcard *.s)
OBJS = $(SRC:%.c=%.o)
AOBJS = $(ASRC:%.s=%.o)
HEADERS = $(SRC:%.c=%.h)

PRG            = main
OBJ            = program.o 
MCU_TARGET     = atmega328
OPTIMIZE       = -O2

# DEFS           = -W1,-u,vprintf -lprintf_flt -lm
DEFS           =
LIBS           = 


# You should not have to change anything below here.

AS		=avr-as
CC             = avr-gcc
CXX             = avr-g++

# Override is only needed by avr-lib build system.

override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS) -I/usr/lib/avr/include
#override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS) -I/usr/local/avr/avr/include

override CXXFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS       = -Wl,-Map,$(PRG).map 

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(SRC) $(HEADERS) $(ASRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PRG).elf $(SRC) $(LIBS)

clean:
	rm -rf *.o $(PRG).elf *.eps *.bak 
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)

install: 
	avrdude -c pony-stk200 -P /dev/parport0 -p m328 -e -U flash:w:$(PRG).hex

usbinstall:
	avrdude -c avrisp2 -P usb -p m328p -e -U flash:w:$(PRG).hex    

tominstall:
	avrdude -c avrisp -P usb -p m328p -e -U flash:w:$(PRG).hex

arduinoinstall0:
	avrdude -c arduino -P /dev/ttyUSB0 -b 57600 -p m328p -e -U flash:w:$(PRG).hex

genuineard0:
	avrdude -c arduino -P /dev/ttyACM0 -b 115200 -p m328p -e -U flash:w:$(PRG).hex

fakeard0:
	avrdude -c arduino -P /dev/ttyACM0 -b 57600 -p m328p -e -U flash:w:$(PRG).hex

fastarduinoinstall0:
	avrdude -c arduino -P /dev/ttyUSB0 -b 115200 -p m328p -e -U flash:w:$(PRG).hex

arduinoinstall1:
	avrdude -c arduino -P /dev/ttyUSB1 -b 57600 -p m328p -e -U flash:w:$(PRG).hex

fastarduinoinstall1:
	avrdude -c arduino -P /dev/ttyUSB1 -b 115200 -p m328p -e -U flash:w:$(PRG).hex

arduinoinstall2:
	avrdude -c arduino -P /dev/ttyUSB2 -b 57600 -p m328p -e -U flash:w:$(PRG).hex

arduinoinstall3:
	avrdude -c arduino -P /dev/ttyUSB3 -b 57600 -p m328p -e -U flash:w:$(PRG).hex

aspinstall:
	avrdude -c usbasp -P usb -p m328p -e -U flash:w:$(PRG).hex

icefuseint:
	avrdude -c atmelice_isp -p m328p -U lfuse:w:0x62:m -U hfuse:w:0xd9:m -U efuse:w:0xfe:m 

usbfuseint:
	avrdude -c avrisp2 -P usb -p m328p -U lfuse:w:0x62:m -U hfuse:w:0xd9:m -U efuse:w:0xfe:m 

usbfuseint8m:
	avrdude -c avrisp2 -P usb -p m328p -U lfuse:w:0xE2:m -U hfuse:w:0xd9:m -U efuse:w:0xfd:m 

usbfuseext:
	avrdude -c avrisp2 -P usb -p m328p -U lfuse:w:0xf7:m -U hfuse:w:0xd9:m -U efuse:w:0x06:m 

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@

# Every thing below here is used by avr-libc's build system and can be ignored
# by the casual user.

FIG2DEV                 = fig2dev
EXTRA_CLEAN_FILES       = *.hex *.bin *.srec



