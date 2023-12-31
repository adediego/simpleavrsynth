import os
import numpy as np


N = 60
header = """// --- file generated by make_saw_table.py ---
//
// sampled sawtooth wave with different amounts
// of low pass filtering. 

#define MAX_TABLE {maxtable}

const uint8_t saw_tables[{n}][256] PROGMEM = {{\
""".format(n = N-1, maxtable = N-2)
footer = "};"

def lowpasssaw(k):
    sawtooth = -0.5 + np.arange(256) / 256
    freqs = np.fft.fft(sawtooth)
    freqs[k+1:-k] = 0
    lowpassed = np.fft.ifft(freqs)
    return np.real(lowpassed)

tables = [lowpasssaw(k) for k in range(1, N)]

# normalise 
max_value = max(map(max, tables))
min_value = min(map(min, tables))
factor = 0.6 / max(max_value, -min_value)
tables = [table * factor for table in tables]

filename = "tables/saw_tables.c"
os.makedirs(os.path.dirname(filename), exist_ok=True)
with open(filename, "w") as file:
    file.write(header)
    for table in tables:
        uint8_table = [round(255 * 0.5 * (s + 1)) for s in table]
        file.write("\n    {\n        ")
        for count, value in enumerate(uint8_table):
            file.write("0x{:02x}, ".format(value))
            if np.mod(count, 8) == 7:
                file.write("\n        ")
        file.write("\n    },\n")
    file.write("};")

