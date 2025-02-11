# simple 8 step sequencer

## Author

dhvani

## Description

This is a simple 8 step sequencer built with Daisy Pod and Patch.init modules. The functionality is split between the modules. Since Daisy Pod's audio out is AC coupled I can't generate a pitch Control Voltage. But I can generate a simple sine wave with the baseline frequency of 440Hz and pass this to Patch.init which can generate a pitch CV since it has a CV out capable to outputting 0-5V.

The Pod uses the encoder click to toggle between edit and view mode and the encoder turn to step through each step in the sequence. A gate signal is generated on the left channel for each step in the sequence. For each active step a pitch can be set using the knob2 which goes down one octave if turned counter clockwise and above the octave if turned clockwise. The limit is only ±octave as of now. An audio signal based on the frequency of the pitch is sent to the right channel.

This is then patched to a Patch.init module. It converts the audio signal to a control voltage with 440Hz as the baseline frequency. It forwards the gate signal without doing any processing on it.

I am testing this sequencer with my Moog Mavis.
