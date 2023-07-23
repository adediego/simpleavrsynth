MCU=atmega328p
PORT=$(shell pavr2cmd --prog-port)
CFLAGS=-g -Wall -mcall-prologues -mmcu=$(MCU) -Os --param=min-pagesize=0
LDFLAGS=-Wl,-gc-sections -Wl,-relax
CC=avr-gcc

TARGET=main

all: $(TARGET).hex $(TARGET).s 

clean: 
	rm -f *.o *.elf *.hex *.s tables/* *.out

%.hex: %.elf
	avr-objcopy -R .eeprom -O ihex $< $@

tables/saw_tables.c: codegen/make_saw_table.py
	python codegen/make_saw_table.py

tables/melody.c: codegen/make_melody.py
	python codegen/make_melody.py

tables/midi.c: codegen/make_midi_table.py
	python codegen/make_midi_table.py

$(TARGET).c: tables/saw_tables.c tables/melody.c tables/midi.c

$(TARGET).elf: $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(TARGET).s: $(TARGET).c
	$(CC) $(CFLAGS) $(LDFLAGS) -S $^

program: $(TARGET).hex
	avrdude -c stk500v2 -P "$(PORT)" -p $(MCU) -U flash:w:$<:i -F

