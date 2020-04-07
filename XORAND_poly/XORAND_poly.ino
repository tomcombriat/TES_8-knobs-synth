/*
   Combriat 2018
   To change from mozzi original in ADSR.h (L51):
       -   unsigned long update_step_counter;
       -   unsigned long update_steps;
       -   unsigned long num_update_steps;
       -   unsigned long convertMsecToControlUpdateSteps(unsigned int msec){
       -   return (uint32_t) (((uint32_t)msec*CONTROL_UPDATE_RATE)>>10); // approximate /1000 with shift
   In MozziConfig.h:
       -   #define AUDIO_RATE 32768

  Compile with -O3 option
  STM32 should work at 128MHz

*/


#define VIBRATO   // comment this line if you want the LFO to act on volume instead of pitch for sine and triangle
// Note that with long release and repeated notes, this make glitchy sounds, because waves are interfering with each other, and we use several oscillators for the same note

#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw_no_alias_2048_int8.h>
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
Oscil<SAW_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> aSaw[POLYPHONY] = Oscil<SAW_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE>(SAW_NO_ALIAS_2048_DATA);
#ifndef VIBRATO
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> LFO[POLYPHONY] = Oscil<COS2048_NUM_CELLS, CONTROL_RATE> (COS2048_DATA);
#else
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> LFO[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE> (COS2048_DATA);
#endif


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


#ifdef VIBRATO
Q15n16 vibrato;
bool mod = true;
#endif


MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);



void set_freq(byte i, bool reset_phase = true)
{
  Q16n16 freq = Q16n16_mtof(Q8n0_to_Q16n16(notes[i]) + (pitchbend << 4));
  aSin[i].setFreq_Q16n16(freq);
  aSquare[i].setFreq_Q16n16(freq);
  aTri[i].setFreq_Q16n16(freq);
  aSaw[i].setFreq_Q16n16(freq);
#ifndef VIBRATO
  if (reset_phase) LFO[i].setPhase(0);
#endif
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


#ifndef VIBRATO
  for (byte i = 0; i < POLYPHONY; i++)
  {
    modulation[i] = (LFO[i].next()) + 1000;
  }
#endif
}

int updateAudio() {

  int sample = 0;

#ifdef VIBRATO
  vibrato = ((Q15n16)  LFO[0].next()) << 4;
#endif

  for (byte i = 0; i < POLYPHONY; i++)
  {
    envelope[i].update();

    int aSin_next;
    int aSquare_next;
    int aTri_next;
    int aSaw_next;

    int partial_sample = 0;

#ifndef VIBRATO
    aSin_next = aSin[i].next();
    aSquare_next = aSquare[i].next();
    aTri_next = aTri[i].next();
    aSaw_next = aSaw[i].next();
#else
    if (!mod)
    {
      aSin_next = aSin[i].next();
      aSquare_next = aSquare[i].next();
      aTri_next = aTri[i].next();
      aSaw_next = aSaw[i].next();
    }
    else
    {
      aSin_next = aSin[i].phMod(vibrato);
      aSquare_next = aSquare[i].next();
      aTri_next = aTri[i].phMod(vibrato);
      aSaw_next = aSaw[i].next();
    }
#endif
    int oscil1 = (((aSin_next * (255 - mix1) + aSquare_next * (mix1)) >> 8 ) * (255 - mix_oscil)) >> 8 ;
    int oscil2 = (((aTri_next * (255 - mix2) + aSaw_next * (mix2)) >> 8 ) * mix_oscil) >> 8;

    int dry = oscil1 + oscil2;
    int wet1 = (oscil1 xor oscil2);
    int wet2 = oscil1 & oscil2;


    partial_sample += ((three_values_knob(wet_dry_mix, 0) >> 1) * wet1) >> 8 ;
    partial_sample += ((three_values_knob(wet_dry_mix, 1) >> 1) * dry) >> 8 ;
    partial_sample += ((three_values_knob(wet_dry_mix, 2) >> 1) * wet2) >> 8 ;
    // sample += ((wet_dry_mix) * wet) >> 8;

#ifndef VIBRATO
    sample += (((partial_sample * envelope[i].next()) >> 8) * modulation[i]) >> 9 ;
#else
    sample += (((partial_sample * envelope[i].next()) >> 8));
#endif
  }



  sample = lpf.next(sample);

  if (sample > 511)
  {
    digitalWrite(LED, HIGH);
    sample = 511;
  }
  else if (sample < -511)
  {
    sample = -511;
  }
  else if (digitalRead(LED)) digitalWrite(LED, LOW);


  return sample;

}

