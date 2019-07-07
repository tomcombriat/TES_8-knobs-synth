/*
   Combriat 2018
   To change from mozzi original:
       -   unsigned long update_step_counter;
       -   unsigned long num_update_steps;   in ADSR.h (L51)

       TODO: velocity and tremolo and sustain
*/



#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <EventDelay.h>
#include <Smooth.h>
#include <ADSR.h>
#include "midi_handles.h"

#define POLYPHONY 10
#define CONTROL_RATE 512 // Hz, powers of 2 are most reliable

#define LED PA8

//Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier[2] = {Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA),Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA)};
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> subOscill[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE>(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> LFO(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> subLFO(COS2048_DATA);

ADSR <AUDIO_RATE, AUDIO_RATE> envelope[POLYPHONY];

Q16n16 deviation;
Q8n8 mod_index;
Q16n16 carrier_freq[POLYPHONY], mod_freq;
Q8n8 mod_to_carrier_ratio ;//= float_to_Q8n8(3.f);
int sub_volume;
int mod;
byte notes[POLYPHONY];
byte runner;

byte oscil_state[POLYPHONY], oscil_rank[POLYPHONY];



Smooth <int> kSmoothInput(0.2f);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);



void set_freq()
{

  for (byte i = 0; i < POLYPHONY; i++)
  {
    mod_freq = ((carrier_freq[i] >> 8) * mod_to_carrier_ratio)  ;
    subOscill[i].setFreq_Q16n16(carrier_freq[i]);
    aModulator[i].setFreq_Q16n16(mod_freq);
  }
}

void set_freq(byte i)
{


  mod_freq = ((carrier_freq[i] >> 8) * mod_to_carrier_ratio)  ;
  subOscill[i].setFreq_Q16n16(carrier_freq[i]);
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


  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  Serial.begin(115200);
  MIDI.turnThruOff ();
  startMozzi(CONTROL_RATE);
}

void loop() {
  audioHook();
}

void updateControl() {

  while (MIDI.read());

  //carrier_freq = Q16n0_to_Q16n16( mozziAnalogRead(PA1)>>1);
  //carrier_freq = float_to_Q16n16(440);
  //mod_to_carrier_ratio =  (mozziAnalogRead(PA3) >> 2 ) << 8 ;
  //Serial.println(mod_to_carrier_ratio);

  sub_volume = mozziAnalogRead(PA7) >> 2;
  //mod_index = float_to_Q8n8(kSmoothInput.next(mozziAnalogRead(PA2)) / 255.);
  mod_index = (kSmoothInput.next(mozziAnalogRead(PA2)));
  deviation = mod_index << 8 ;

  /*for (byte i = 0; i < POLYPHONY; i++)*/ envelope[runner].setTimes(mozziAnalogRead(PA6), 1, 65000, mozziAnalogRead(PA3));
  runner++;
  if (runner >= POLYPHONY) runner = 0;

  
LFO.setFreq_Q16n16((Q16n16) (mozziAnalogRead(PA4) << 8 ));
//Serial.println(Q16n16_to_float((Q16n16) (mozziAnalogRead(PA4) << 8 )));
mod = (300 + LFO.next()) >> 3;
//Serial.println(modulation);


  

}

int updateAudio() {
  int sample = 0;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    envelope[i].update();
    Q15n16 modulation = deviation * aModulator[i].next() >> 8;
    sample += (int)   (envelope[i].next() * (((((aCarrier[i].phMod(modulation)  ) * mod) >> 1)      + ((sub_volume * subOscill[i].next()) >> 6  ) ) >> 8)  >> 6)  ;
    //                       8                    + 8                                    +9           -2        -1              +8            +8 
    //sample += (int)  (( envelope[i].next() * ((aCarrier[i].phMod(modulation) << 2) + (sub_volume * subOscill[i].next() >> 8 ) ) ) >> 12) /*+ sub_volume * subOscill[i].next()*/;
  }

  //Q15n16 modulation = deviation * aModulator.next() >> 8;
  // return (int)  ( envelope[0].next() * (aCarrier[0].phMod(modulation) << 2) >> 8);
  return sample;
}

