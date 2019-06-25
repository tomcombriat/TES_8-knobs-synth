
#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <EventDelay.h>
#include <Smooth.h>


#define CONTROL_RATE 256 // Hz, powers of 2 are most reliable

Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);

Q16n16 deviation;
  int mod_index;
  Q16n16 carrier_freq, mod_freq;
  Q8n8 mod_to_carrier_ratio = float_to_Q8n8(3.f);


MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);




void setup() {
  pinMode(PA1, INPUT);
  pinMode(PA2, INPUT);
  pinMode(PA3, INPUT);
  pinMode(PA4, INPUT);
  pinMode(PA5, INPUT);
  pinMode(PA6, INPUT);
  pinMode(PA7, INPUT);
  pinMode(PA8, OUTPUT);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  Serial.begin(115200);
    startMozzi(CONTROL_RATE);
}

void loop() {
  audioHook();
}

void updateControl(){
  carrier_freq = Q16n0_to_Q16n16( mozziAnalogRead(PA1)>>2);
  //carrier_freq = float_to_Q16n16(440);
deviation = ((mod_freq>>16) * mod_index);
mod_index = float_to_Q8n8(mozziAnalogRead(PA2)/500.); 
   mod_to_carrier_ratio = float_to_Q8n8(mozziAnalogRead(PA3)/500.);
mod_freq = ((carrier_freq>>8) * mod_to_carrier_ratio)  ;
  aCarrier.setFreq_Q16n16(carrier_freq);
  aModulator.setFreq_Q16n16(mod_freq);
}

int updateAudio(){
  Q15n16 modulation = deviation * aModulator.next() >> 8;
  return (int)aCarrier.phMod(modulation);
}

