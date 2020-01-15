# TES - 8 knobs synth
T. Combriat

8 knobs synths are simple synthesizers based on [Mozzi](https://github.com/sensorium/Mozzi) using STM32 blue pill as the main hardware. Thanks to the computing power of these chips, these synthesizers are able to generate complex polyphonic sounds at 32kHz sampling rate. The aim of this serie is to provide a fixed hardware which can be used for different synthesis methods.



### Hardware
These synthesizers feature: 
* MIDI input and thru
* 8 knobs for controlling parameters (thus the name), note that one is hardwired for volume control
* 6.35 audio output (mono)

PCBs for the chip are single faced and can be easily made if one is able to make 250 microns wide tracks. The mask is provided here.



### Versions
For now, only one version of this synth has been developped: mono output using the chip PWM and a low-pass 1<sup>st</sup> filter. Other versions, featuring Mozzi HiFi, stereo, DAC outputs may come.

### Programming
These synths can be programmed like any other STM32 pills using USB to serial converter. You will need a rather new version of the Arduino IDE with Mozzi installed.
