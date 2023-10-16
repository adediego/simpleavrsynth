# Simple AVR synthesizer

A wavetable-based synthesizer on an Atmega328P capable of basic MIDI (note on/off).

## Features
   - 16kHz sample rate
   - audio signal in 8 bit fixed point precision
   - sawtooth signal stored in flash memory as low-pass filtered wavetables to avoid aliasing.
     Tables with different filter cutoffs are generated in Python before compilation.
     The appropriate table for some note is selected at runtime.
   - 6 independent voices 

## Circuit diagram
<img style="background-color:#ffffff;" src="./media/circuit.svg">

## Sound example
[example](https://github.com/adediego/simpleavrsynth/assets/38465681/035a1826-e39e-42b3-be77-b7c4a93e92e5)


## Notes
The `CKDIV8` bit in the low fuse byte (FLB) needs to be unprogrammed (set to `1`) to set the clock frequency to `8 MHz`.
With the other bits left to default, this results in a value of `0xE2` for FLB.
