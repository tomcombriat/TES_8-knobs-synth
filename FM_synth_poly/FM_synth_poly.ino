/*
   Combriat 2018
   To change from mozzi original:
       -   unsigned long update_step_counter;
       -   unsigned long update_steps;
       -   unsigned long num_update_steps;   in ADSR.h (L51)
       -   unsigned long convertMsecToControlUpdateSteps(unsigned int msec){
   return (uint32_t) (((uint32_t)msec*CONTROL_UPDATE_RATE)>>10); // approximate /1000 with shift


*/



#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <tables/square_no_alias_2048_int8.h>
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <EventDelay.h>
#include <Smooth.h>
#include <ADSR.h>
#include <LowPassFilter.h>
#include "midi_handles.h"

#define POLYPHONY 16
#define CONTROL_RATE 512 // Hz, powers of 2 are most reliable

#define LED PA8

//Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier[2] = {Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA),Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA)};
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier[POLYPHONY] = {Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA)};
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator[POLYPHONY] = {Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA)};
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> subOscill[POLYPHONY] = {Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA)};
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> subOscill_sq[POLYPHONY] = {Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA)};
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> LFO(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> subLFO(COS2048_DATA);

ADSR <AUDIO_RATE, AUDIO_RATE> envelope[POLYPHONY];
LowPassFilter lpf;

Q16n16 deviation;
Q8n8 mod_index;
Q16n16 carrier_freq[POLYPHONY], mod_freq;
Q8n8 mod_to_carrier_ratio ;//= float_to_Q8n8(3.f);
int sub_volume;
int pitchbend = 0;
int mod, mod_sub;
byte notes[POLYPHONY];
byte runner, aftertouch = 0, modulationW = 127;
byte oscil_state[POLYPHONY], oscil_rank[POLYPHONY];
unsigned long last_sustain_time;
bool sustain = false;
int mix_sub;


Smooth <int> kSmoothInput(0.2f);

// Serial1 RX PA10
// Serial2 RX PA3
// Serial3 RX PB11

MIDI_CREATE_INSTANCE(HardwareSerial, Serial3, MIDI);


void set_freq(byte i)
{
  carrier_freq[i] = Q16n16_mtof(Q8n0_to_Q16n16(notes[i]) + (pitchbend << 4));
  mod_freq = ((carrier_freq[i] >> 8) * mod_to_carrier_ratio)  ;
  aCarrier[i].setFreq_Q16n16(carrier_freq[i]);
  subOscill[i].setFreq_Q16n16(carrier_freq[i]);
  subOscill_sq[i].setFreq_Q16n16(carrier_freq[i]);
  aModulator[i].setFreq_Q16n16(mod_freq);
}



void setup() {
  pinMode(PA1, INPUT);
  pinMode(PA2, INPUT);
  pinMode(PA3, INPUT);
  pinMode(PA4, INPUT);
  pinMode(PA5, INPUT);
  pinMode(PA6, INPUT);
  pinMode(PA7, INPUT);
  pinMode(LED, OUTPUT);

  for (byte i = 0; i < POLYPHONY; i++)
  {
    oscil_state[i] = 0;   //0: unactive  1: playing   2: playing thanks to sustain
    oscil_rank[i] = 0;
  }
  runner = 0;

  for (byte i = 0; i < POLYPHONY; i++) envelope[i].setADLevels(255, 255);
  mod_to_carrier_ratio = float_to_Q8n8(3.f);
  lpf.setResonance(20);


  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);
  MIDI.setHandleControlChange(HandleControlChange);
  MIDI.setHandlePitchBend(HandlePitchBend);
  MIDI.setHandleAfterTouchChannel(HandleAfterTouchChannel);

  MIDI.begin(1);
  //Serial.begin(115200);
  MIDI.turnThruOff ();
  startMozzi(CONTROL_RATE);
}

void loop() {
  audioHook();
}

void updateControl() {

  while (MIDI.read());



  sub_volume = mozziAnalogRead(PA7) >> 2;
  mod_index = (kSmoothInput.next(mozziAnalogRead(PA3)));
  deviation = mod_index << 8 ;

  envelope[runner].setTimes(mozziAnalogRead(PA2), 1, 65000, mozziAnalogRead(PA1));
  runner++;
  if (runner >= POLYPHONY) runner = 0;

  LFO.setFreq_Q16n16((Q16n16) (mozziAnalogRead(PA6) << 8 ));
  subLFO.setFreq_Q16n16((Q16n16) (mozziAnalogRead(PA5) << 8 ));

  mod = (400 + LFO.next()) >> 4;
  mod_sub   = (350 + subLFO.next()) >> 4;

  mix_sub = (mozziAnalogRead(PA4) >> 4 );

  if ((int) (modulationW << 1) + (aftertouch) < 255) lpf.setCutoffFreq((modulationW << 1) + (aftertouch) );
  else lpf.setCutoffFreq(255);



}

int updateAudio() {
  int sample = 0;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    envelope[i].update();
    Q15n16 modulation = deviation * aModulator[i].next() >> 8;
    sample += (int)   (envelope[i].next() * (((((aCarrier[i].phMod(modulation)  ) * mod) >> 2)      + ((((sub_volume * (((255 - mix_sub) * subOscill[i].next() + (mix_sub) * subOscill_sq[i].next()) >> 8)) >> 8) * mod_sub) >> 4)) >> 8)  >> 2)  ;
  }
  sample = lpf.next(sample);
  if (sample > 511)
  {
    digitalWrite(LED, HIGH);
    sample = 511;
  }
  else if (sample < -511) sample = -511;
  return sample;
}

