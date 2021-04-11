/*
   Combriat 2020
   To change from mozzi original:
       -   unsigned long update_step_counter;
       -   unsigned long update_steps;
       -   unsigned long num_update_steps;   in ADSR.h (L51)
       -   unsigned long convertMsecToControlUpdateSteps(unsigned int msec){
   return (uint32_t) (((uint32_t)msec*CONTROL_UPDATE_RATE)>>10); // approximate /1000 with shift


*/

// TO BE LOADED
// to be tested : *log* volume = 255*(1-exp(-0.04*breath))  LOADED
// to be tested : *exp* volume = exp(breath/22.9)


#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>

#include <tables/triangle2048_int8.h>
#include <mozzi_midi.h>
#include <mozzi_fixmath.h>
#include <Smooth.h>
#include <ADSR.h>
#include <LowPassFilter.h>
#include <AudioDelayFeedback.h>
#include <Portamento.h>
#include "midi_handles.h"

#define POLYPHONY 8
#define CONTROL_RATE 2048 // Hz, powers of 2 are most reliable

#define LED PA8
#define BREATH_LIN
//#define BREATH_LOG
//#define BREATH_EXP



Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aSin[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE> (COS2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> aSquare[POLYPHONY] = Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE>(SQUARE_NO_ALIAS_2048_DATA);
Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> aTri[POLYPHONY] = Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE>(TRIANGLE2048_DATA);
Oscil<SAW_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE> aSaw[POLYPHONY] = Oscil<SAW_NO_ALIAS_2048_NUM_CELLS, AUDIO_RATE>(SAW_NO_ALIAS_2048_DATA);
//Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> aSaw[POLYPHONY] = Oscil<SAW2048_NUM_CELLS, AUDIO_RATE>(SAW2048_DATA);
//Oscil<COS2048_NUM_CELLS, CONTROL_RATE> LFO[POLYPHONY] = Oscil<COS2048_NUM_CELLS, CONTROL_RATE> (COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> LFO[POLYPHONY] = Oscil<COS2048_NUM_CELLS, AUDIO_RATE> (COS2048_DATA);


ADSR <AUDIO_RATE, AUDIO_RATE> envelope[POLYPHONY];
ADSR <AUDIO_RATE, AUDIO_RATE> envelope_audio;
LowPassFilter lpf;
Smooth <int> kSmoothInput(0.2f);
Smooth <byte> breath_smooth(0.6f);
//Portamento<CONTROL_RATE> porta;

byte notes[POLYPHONY] = {0};
int wet_dry_mix, modulation[POLYPHONY];
int mix1;
int mix2;
int mix_oscil, cutoff = 0, pitchbend = 0, pitchbend_amp = 2, aftertouch = 0, prev_cutoff = 0, breath_on_cutoff = 0, midi_cutoff = 255;
byte oscil_state[POLYPHONY], oscil_rank[POLYPHONY], runner = 0, volume = 0, delay_volume = 0;
bool sustain = false;
bool mod = true;
bool osc_is_on[POLYPHONY] = {false};
unsigned int chord_attack = 1, chord_release = 1;
byte breath_to_volume[128];
bool toggle = 0;
Q15n16 vibrato;



// Serial1 RX PA10
// Serial2 RX PA3
// Serial3 RX PB11

MIDI_CREATE_INSTANCE(HardwareSerial, Serial3, MIDI);



void set_freq(byte i, bool reset_phase = false)
{

  osc_is_on[i] = true;
  Q16n16 freq = Q16n16_mtof(Q8n0_to_Q16n16(notes[i]) + (pitchbend << 3) * pitchbend_amp);
  //Q16n16 freq = porta.next() + Q16n16_mtof(pitchbend << 4) ; // mettre plutot   Q16n16_mtof((pitchbend << 4) + porta.next())

  //Q16n16 freq = Q16n16_mtof(Q8n0_to_Q16n16(notes[i]));
  //Serial.println(Q16n16_to_float(modulation[i]));
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
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  pinMode(PA1, INPUT);
  pinMode(PA2, INPUT);
  pinMode(PA3, INPUT);
  pinMode(PA4, INPUT);
  pinMode(PA5, INPUT);
  pinMode(PA6, INPUT);
  pinMode(PA7, INPUT);

  for (byte i = 0; i < POLYPHONY; i++)
  {
    aSaw[i].setPhase(2048 >> 2 );
    envelope[i].setADLevels(128, 128);
    envelope[i].setTimes(1, 1, 65000, 10);
  }

  for (int i = 0; i < 128; i++)
  {
#ifdef BREATH_LOG
    breath_to_volume[i] = 255 * (1 - exp(-0.04 * i));
#endif
#ifdef BREATH_LIN
    breath_to_volume[i] = 2 * i;
#endif
#ifdef BREATH_EXP
    breath_to_volume[i] = exp(i / 23.);
#endif

  }
  // envelope_audio.setADLevels(128, 128);
  //envelope_audio.setTimes(1, 1, 65000, 100);
  lpf.setResonance(25);



  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);
  MIDI.setHandleControlChange(HandleControlChange);
  MIDI.setHandlePitchBend(HandlePitchBend);
  MIDI.setHandleAfterTouchChannel(HandleAfterTouchChannel);

  //Serial.begin(115200);


  MIDI.begin(MIDI_CHANNEL_OMNI);
  delay(100);
  digitalWrite(LED, LOW);
  delay(100);
  digitalWrite(LED, HIGH);
  delay(100);

  MIDI.turnThruOff (); //for speed
  startMozzi(CONTROL_RATE);
  digitalWrite(LED, LOW);

}







void loop() {
  audioHook();
}







void updateControl() {

  while (MIDI.read());
  //set_freq(0);
  //Serial.println(volume);
  if (toggle)
  {
    mix1 =  mozziAnalogRead(PA6) >> 4;
    mix2 =  mozziAnalogRead(PA5) >> 4;
    wet_dry_mix = mozziAnalogRead(PA7) >> 2;  // goos to 1024
    mix_oscil = mozziAnalogRead(PA3) >> 4 ;
  }
  else
  {
    chord_release = mozziAnalogRead(PA1) >> 1 ;
    chord_attack = mozziAnalogRead(PA2) >> 1 ;
    breath_on_cutoff = kSmoothInput(mozziAnalogRead(PA4) >> 4);
    cutoff = ((breath_on_cutoff * volume) >> 7 ) + midi_cutoff;

    if (cutoff > 255) cutoff = 255;

    if (cutoff != prev_cutoff)
    {
      lpf.setCutoffFreq(cutoff);
      prev_cutoff = cutoff;
    }
  }
  toggle = !toggle;

  /*
    for (byte i = 0; i < POLYPHONY; i++)
    {
      modulation[i] = (LFO[i].next());
    }
  */
  // MIDI.read();
}

int updateAudio() {

  long sample = 0;
  //envelope_audio.update();



  int breath_next = breath_smooth.next(breath_to_volume[volume]); // this could be done in updatecontrol() maybe? for speed? And the following also
  if (breath_next == 0)
  {
    for (byte i = 0; i < POLYPHONY; i++)
    {
      envelope[i].noteOff();
      osc_is_on[i] = false; // so that chord do not fade out during next note afer a short pause
      oscil_state[i] = 0;   // everybody reset
    }
  }


  //vibrato = (Q15n16) 32 * LFO[0].next();
  vibrato = ((Q15n16)  LFO[0].next()) << 4;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    envelope[i].update();
    int env_next = envelope[i].next();  // for enveloppe to roll even if it is not playing
    if (envelope[i].playing() && osc_is_on[i])
    {
      long partial_sample = 0;

      int aSin_next;
      int aSquare_next;
      int aTri_next;
      int aSaw_next;


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


      int oscil1 = (((aSin_next * (255 - mix1) + aSquare_next * (mix1)) >> 8 ) * (255 - mix_oscil)) >> 8 ;
      int oscil2 = (((aTri_next * (255 - mix2) + aSaw_next * (mix2)) >> 8 ) * mix_oscil) >> 8;

      int dry = oscil1 + oscil2;
      int wet1 = (oscil1 xor oscil2);
      int wet2 = oscil1 & oscil2;


      partial_sample += ((three_values_knob(wet_dry_mix, 0) >> 1) * wet1) >> 8 ;
      partial_sample += ((three_values_knob(wet_dry_mix, 1) >> 1) * dry) >> 8 ;
      partial_sample += ((three_values_knob(wet_dry_mix, 2) >> 1) * wet2) >> 8 ;
      // sample += ((wet_dry_mix) * wet) >> 8;

      //sample += (((((partial_sample * (breath_to_volume[volume])) >> 8) * modulation[i]) >> 8) * envelope_audio.next()) >> 8 ; //is played actively now
      //sample += (((((partial_sample * (breath_to_volume[volume])) >> 8) * modulation[i]) >> 7) ); //is played actively now
      //sample += envelope_audio.next() * (((((partial_sample * (breath_smooth.next(breath_to_volume[volume]))) ) * modulation[i]) >> 16)); //is played actively now

      //sample +=  (partial_sample * (breath_next * envelope[i].next()) >> 2)  >> 10;
      sample += (partial_sample * env_next) >> 3;

      //else sample += (((partial_sample * (envelope[i].next())) >> 8) * modulation[i]) >> 9 ;  //is played actively now
    }
  }

  sample = (sample * breath_next) >> 10 ;


  // int env = envelope[0].next();
  //int env = envelope_audio.next();



  /*
    sample += (((delay_level * aDel.next(byte(sample >> 2), ((Q16n16) 2048) << 16 )) >> 8) * env) >> 8 ;
    sample += (((delay_level * aDel2.next(byte(sample >> 2), ((Q16n16) 1801) << 16 )) >> 10) * env) >> 8 ;

  */

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

