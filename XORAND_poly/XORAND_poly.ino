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
#include <tables/saw2048_int8.h>
#include <tables/triangle2048_int8.h>
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <Smooth.h>
#include <ADSR.h>
#include <LowPassFilter.h>

#include "midi_handles.h"

#define POLYPHONY 16
#define CONTROL_RATE 1024 // Hz, powers of 2 are most reliable

#define LED PA8


Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aSin[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE> (COS2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> aSquare[POLYPHONY] = Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA);
Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> aTri[POLYPHONY] = Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE>(TRIANGLE2048_DATA);
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> aSaw[POLYPHONY] = Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>(SAW2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> LFO[POLYPHONY] = Oscil<COS2048_NUM_CELLS, CONTROL_RATE> (COS2048_DATA);



ADSR <AUDIO_RATE, AUDIO_RATE> envelope[POLYPHONY];
LowPassFilter lpf;
Smooth <int> kSmoothInput(0.2f);


byte notes[POLYPHONY] = {0};
int wet_dry_mix, modulation[POLYPHONY];
int mix1;
int mix2;
int mix_oscil, cutoff = 0, pitchbend = 0, aftertouch = 0, prev_cutoff = 0;
byte oscil_state[POLYPHONY], oscil_rank[POLYPHONY], runner = 0;
bool sustain = false;



MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);



void set_freq(byte i, bool reset_phase = true)
{
  Q16n16 freq = Q16n16_mtof(Q8n0_to_Q16n16(notes[i]) + (pitchbend << 4));
  aSin[i].setFreq_Q16n16(freq);
  aSquare[i].setFreq_Q16n16(freq);
  aTri[i].setFreq_Q16n16(freq);
  aSaw[i].setFreq_Q16n16(freq);

  if (reset_phase) LFO[i].setPhase(0);
}


int three_values_knob(int val, int i)
{
  switch (i)
  {
    case 0:
      {
        if (val > 512) {
          return 0;
        }
        else return (512 - val);
      }
    case 1:
      {
        if (val < 512) return val;
        else return 1024 - val;
      }
    case 2:
      {
        if (val < 512 ) return 0;
        else return val - 512;
      }
  }
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
    aSaw[i].setPhase(2048 >> 2 );
    envelope[i].setADLevels(255, 255);
  }
  lpf.setResonance(25);

  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);
  MIDI.setHandleControlChange(HandleControlChange);
  MIDI.setHandlePitchBend(HandlePitchBend);
  MIDI.setHandleAfterTouchChannel(HandleAfterTouchChannel);

  MIDI.begin(2);
  //Serial.begin(115200);
  MIDI.turnThruOff ();
  startMozzi(CONTROL_RATE);
}

void loop() {
  audioHook();
}

void updateControl() {

  while (MIDI.read());


  mix1 =  mozziAnalogRead(PA6) >> 4;
  mix2 =  mozziAnalogRead(PA5) >> 4;
  wet_dry_mix = mozziAnalogRead(PA7) >> 2;  // goos to 1024
  mix_oscil = mozziAnalogRead(PA3) >> 4 ;
  cutoff = kSmoothInput(mozziAnalogRead(PA4) >> 4);
  if (cutoff + aftertouch > 255) cutoff =  255;
  else cutoff += aftertouch;
  //cutoff = 12;
  if (cutoff != prev_cutoff)
  {
    lpf.setCutoffFreq(cutoff);
    prev_cutoff = cutoff;
  }
 
  runner++;
  if (runner >= POLYPHONY) runner = 0;
  envelope[runner].setTimes(mozziAnalogRead(PA2), 1, 65000, mozziAnalogRead(PA1));
  //envelope[runner].setTimes(mozziAnalogRead(PA2), 6000, 65000, mozziAnalogRead(PA1));

  for (byte i = 0; i < POLYPHONY; i++)
  {
    modulation[i] = (LFO[i].next()) + 1000;
  }
}

int updateAudio() {

  int sample = 0;

  for (byte i = 0; i < POLYPHONY; i++)
  {
    envelope[i].update();

    int partial_sample = 0;
    int aSin_next = aSin[i].next();
    int aSquare_next = aSquare[i].next();
    int aTri_next = aTri[i].next();
    int aSaw_next = aSaw[i].next();

    int oscil1 = (((aSin_next * (255 - mix1) + aSquare_next * (mix1)) >> 8 ) * (255 - mix_oscil)) >> 8 ;
    int oscil2 = (((aTri_next * (255 - mix2) + aSaw_next * (mix2)) >> 8 ) * mix_oscil) >> 8;

    int dry = oscil1 + oscil2;
    int wet1 = (oscil1 xor oscil2);
    int wet2 = oscil1 & oscil2;


    partial_sample += ((three_values_knob(wet_dry_mix, 0) >> 1) * wet1) >> 8 ;
    partial_sample += ((three_values_knob(wet_dry_mix, 1) >> 1) * dry) >> 8 ;
    partial_sample += ((three_values_knob(wet_dry_mix, 2) >> 1) * wet2) >> 8 ;
    // sample += ((wet_dry_mix) * wet) >> 8;

    sample += (((partial_sample * envelope[i].next()) >> 8) * modulation[i]) >> 9 ;
  }
  /*
    if (sample > 511)    sample = 511;
    else if (sample < -511)    sample = -511;
  */


  sample = lpf.next(sample);

  if (sample > 511)
  {
    digitalWrite(LED, HIGH);
    Serial.println("over!");
    sample = 511;
  }
  else if (sample < -511)
  {
    sample = -511;
  }
  else if (digitalRead(LED)) digitalWrite(LED, LOW);


  return sample;

}
