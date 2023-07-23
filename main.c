#include <avr/io.h>
#include <avr/interrupt.h>
#include <limits.h>
#include <avr/pgmspace.h>

#include "tables/melody.c"
#include "tables/saw_tables.c"

// SPI definitions
#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define CS      PINB2
#define MOSI    PINB3
#define MISO    PINB4
#define SCK     PINB5

// DAC bit positions
#define DAC_IGNORE  15 
#define DAC_BUF     14
#define DAC_NGA     13
#define DAC_NSHDN   12
#define DAC_DATA_SH  4 

volatile uint16_t osc1_acc = 0;
volatile uint16_t osc2_acc = 0;
volatile uint16_t osc1_step = 0;
volatile uint16_t osc2_step = 0;

volatile int table1 = 0;
volatile int table2 = 0;

volatile int note_count = 0;

int select_table(uint16_t step);

void update_oscs() {
    // update oscillator frequencies
    osc1_step = steps1[note_count];
    osc2_step = steps2[note_count];
    
    // update the lookup table for the samples
    table1 = select_table(osc1_step);
    table2 = select_table(osc2_step);
}


void setup_SPI()
{
    //set CS, MOSI and SCK to output
    SPI_DDR |= (1 << CS) | (1 << MOSI) | (1 << SCK);

    // enable SPI, set as master and set clock to fosc/128
    SPCR = (1 << SPE) | (1 << MSTR) | (0 << SPR1) | (0 << SPR0);

    SPSR |= (1<< SPI2X);  // double speed
    SPCR &= ~(1 << DORD); // most significant bit first

    SPDR = 0; 
    SPI_PORT |= (1 << CS); // idle chip select
}

void dac_write(uint8_t value)
{
    uint16_t data = 
                  (1 << DAC_NSHDN) |
                  (1 << DAC_NGA)   |
                  (((uint16_t) value) << DAC_DATA_SH);

    uint8_t spi_byte1 = data >> 8;
    uint8_t spi_byte2 = data & 0xff;

    // drive slave select low 
    SPI_PORT &= ~(1 << CS);

    // transmit first byte
    SPDR = spi_byte1;
    while(!(SPSR & (1 << SPIF)));

    // transmit second byte
    SPDR = spi_byte2;
    while(!(SPSR & (1 << SPIF)));

    // drive slave select high
    SPI_PORT |= (1 << CS);
}

void setup(){
    setup_SPI();

    cli();

    //set timer0  at 16kHz 
    TCCR0A |= (1 << WGM01);  // CTC Mode
    TCCR0B |= (1 << CS01);   // 8x prescaler
    OCR0A   = 61;            // results in 16kHz
    TIMSK0 |= (1 << OCIE0A); // enable interrupt

    //set timer1 at 4Hz
    TCCR1B |= (1 << WGM12);  // CTC Mode
    TCCR1B |= (1 << CS12);   // 256x prescaler
    OCR1A   = 7575;          // results in 4 Hz
    TIMSK1 |= (1 << OCIE1A); // enable interrupt

    // allow interrupts
    sei();
    update_oscs();
}

// Low frequency interrupt for note changes
ISR(TIMER1_COMPA_vect){
    // allow interrupt by audio timer
    sei();

    note_count += 1;
    note_count = note_count % mel_length;

    update_oscs();
}


// High frequency interrupt for audio output
ISR(TIMER0_COMPA_vect){
    // increment oscillator accumulators:
    osc1_acc += osc1_step;
    osc2_acc += osc2_step;

    int sample_k_1 = (osc1_acc >> 8 ) & 0xff;
    int sample_k_2 = (osc2_acc >> 8 ) & 0xff;
    uint16_t val1 =  pgm_read_byte(&(saw_tables[table1][sample_k_1]));
    uint16_t val2 =  pgm_read_byte(&(saw_tables[table2][sample_k_2]));

    uint16_t val = (val1 + val2) >> 1;
    dac_write(val & 0xff);
}

// return the right lookuptable for a given frequency
// such that there are no frequencies in the signal 
// that would alias to lower frequencies.
int select_table(uint16_t step)
{
    // silence (and step 0 would result in ub below)
    if (step == 0)
        return 0;
    
    int table = (1<<15) / step - 1;
    if ( table > MAX_TABLE )
        table = MAX_TABLE;
    if ( table < 0)
        table = 0;
    return table;
}


int main() {
    DDRB |= (1 << 0);
    setup();
    update_oscs();

    while(1){
    }
}

