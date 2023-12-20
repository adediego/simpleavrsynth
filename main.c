#include <avr/io.h>
#include <avr/interrupt.h>
#include <limits.h>
#include <avr/pgmspace.h>
#include <stdbool.h>

//#include "tables/melody.c"
#include "tables/saw_tables.c"
#include "tables/midi.c"

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


#define NVOICES 6
#define SCALEEXPONENT 3
volatile bool osc_on[NVOICES];
volatile uint16_t table[NVOICES];
volatile uint8_t  osc_note[NVOICES];
volatile uint16_t osc_step[NVOICES];
volatile uint16_t osc_acc[NVOICES];

volatile int note_count = 0;

volatile uint8_t midi_status = 0;
volatile uint8_t midi_data_byte = 0;

int select_table(uint16_t step);
void handle_message(uint8_t message);

void init_oscillators(){
    for (int k = 0; k<NVOICES; k++)
    {
        osc_note[k] = 0;
        osc_step[k] = 0;
        osc_acc[k] = 0;
        table[k] = 0;
    }
}


void update_oscs() {
    // update the lookup table for the samples
    for (int k = 0; k< NVOICES; k++)
    {
        table[k] = select_table(osc_step[k]);
    }
}


void init_SPI()
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

void init_timer(){
    cli();

    //set timer0  at 16kHz 
    TCCR0A |= (1 << WGM01);  // CTC Mode
    TCCR0B |= (1 << CS01);   // 8x prescaler
    OCR0A   = 61;            // results in 16kHz
    TIMSK0 |= (1 << OCIE0A); // enable interrupt
                             //
    sei();
}


// interrupt for audio output
ISR(TIMER0_COMPA_vect){
    // increment oscillator accumulators:
    cli();
    uint16_t val = 0;
    for (int k = 0; k < NVOICES; k++){
        osc_acc[k] += osc_step[k];
        uint8_t sample = (osc_acc[k] >> 8 ) & 0xff;
        val += pgm_read_byte(&(saw_tables[table[k]][sample]));
    }
    val = val >> SCALEEXPONENT;
    dac_write(val & 0xff);
    sei();
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

void init_usart(unsigned int ubrr)
{
    /*Set baud rate */
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;

    // Enable receiver and interrupt 
    UCSR0B = (1<<RXEN0) | (1<<RXCIE0); 

    // Set frame format: 8data, 1stop bit 
    UCSR0C = (3<<UCSZ00);

}

ISR(USART_RX_vect){
     handle_message(UDR0);
}


void note_on(uint8_t note){
    for (int k = 0; k < NVOICES; k++)
    {
        if (!osc_on[k])
        {
            osc_on[k] = true;
            osc_step[k] = pgm_read_word(&(midi_to_step[note]));
            osc_note[k] = note;
            update_oscs();
            return;
        }
    }
}

void note_off(uint8_t note){
    for (int k = 0; k < NVOICES; k++)
    {
        if (osc_on[k] && (osc_note[k] == note))
        {
            osc_on[k] = false;
            osc_step[k] = 0; 
            return;
        }
    }
}

void handle_data(uint8_t data){
    if ((midi_status == 0x90) && (midi_data_byte == 0)){
        if (data < 128)
            note_on(data);
    }
    else {
        if (midi_status == 0x80){
            note_off(data);
        }
    }
    midi_data_byte++;
}

void handle_message(uint8_t message){
    if ( message & (1 << 7)) // status byte?
        switch (message){
            case 0x80: // note off
            case 0x90: // note on 
                midi_status = message;
                midi_data_byte = 0;
                break;
            default:   // reset state if other message
                midi_status = 0;
                midi_data_byte = 0;
        }
    else
        handle_data(message);
}


int main() {
    cli();
    init_oscillators();
    init_usart(15);
    init_SPI();
    init_timer();
    sei();


    while(1){
    }
}

