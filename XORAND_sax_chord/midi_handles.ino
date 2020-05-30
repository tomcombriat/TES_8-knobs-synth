#include "midi_handles.h"



/*********************
       NOTE ON
 ********************/
void HandleNoteOn(byte channel, byte note, byte velocity)
{
  //envelope_audio.noteOn();
  byte min_rank = 255;
  int empty_arg = -1;
  for (byte i = 0; i < POLYPHONY; i++)  //take a non playing oscil
  {
    if (!envelope[i].playing() && oscil_rank[i] < min_rank)
    {
      empty_arg = i;
      min_rank = oscil_rank[i];
    }
  }

  if (empty_arg == -1)  //kill a oscil in release phase
  {
    min_rank = 255;
    for (byte i = 0; i < POLYPHONY; i++)
    {
      if (oscil_state[i] == 0 && oscil_rank[i] < min_rank)
      {
        empty_arg = i;
        min_rank = oscil_rank[i];
      }
    }
  }


  if (empty_arg == -1)  // no empty oscil, kill one in sustain mode! (the oldest)
  {
    min_rank = 255;
    byte min_rank_arg = 0;
    for (byte i = 0; i < POLYPHONY; i++)
    {
      if (oscil_rank[i] < min_rank)
      {
        min_rank = oscil_rank[i];
        min_rank_arg  = i;
      }
    }
    empty_arg = min_rank_arg;
  }

  bool chord_note = false;
  envelope[empty_arg].setAttackTime(1);
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (oscil_state[i] == 1)
    {
      envelope[empty_arg].setAttackTime(chord_attack);
      chord_note = true;
      break;
    }
  }
  notes[empty_arg] = note;
  set_freq(empty_arg);

  if (chord_note) envelope[empty_arg].noteOn(true);
  else envelope[empty_arg].noteOn();
  oscil_state[empty_arg] = 1;




  byte max_rank = 0;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (oscil_rank[i] > max_rank) max_rank = oscil_rank[i];
  }
  oscil_rank[empty_arg] = max_rank + 1;


  // shift all oscill
  min_rank = 255;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (oscil_rank[i] < min_rank)
    {
      empty_arg = i;
      min_rank = oscil_rank[i];
    }
  }
  if (min_rank != 0) for (byte i = 0; i < POLYPHONY; i++) oscil_rank[i] -= min_rank;

  volume = velocity;
}


/**********************
      NOTE OFF
 *********************/


void HandleNoteOff(byte channel, byte note, byte velocity)
{
  //volume = 0;
  //envelope[0].noteOff();
  //envelope_audio.noteOff();
  byte to_kill = 255;
  byte min_rank = 255;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (note == notes[i] && oscil_state[i] == 1 && oscil_rank[i] < min_rank  )
    {
      min_rank = oscil_rank[i];
      to_kill = i;
    }
  }

  if (to_kill != 255)
  {
    envelope[to_kill].setReleaseTime(1);
    if (!sustain)
    {
      for (byte i = 0; i < POLYPHONY; i++)
      {
        if (i != to_kill && oscil_state[i] == 1)
        {
          envelope[to_kill].setReleaseTime(chord_release);
          break;
        }
      }
      envelope[to_kill].noteOff();
      oscil_state[to_kill] = 0;
    }
    else oscil_state[to_kill] = 2;

    //envelope[0].noteOff();
    for (byte i = 0; i < POLYPHONY; i++)
    {
      if (oscil_rank[i] > oscil_rank[to_kill]) oscil_rank[i] -= 1;
    }
  }
}


/*************************
      CONTROL CHANGE
 ************************/
void HandleControlChange(byte channel, byte control, byte val)
{
  switch (control)
  {
    case 64:    // sustain
      if (val == 0)
      {
        sustain = false;
        for (byte i = 0; i < POLYPHONY; i++)
        {
          if (oscil_state[i] == 2)
          {
            envelope[i].noteOff();
            oscil_state[i] = 0;
          }
        }
      }
      else sustain = true;
      break;


    case 1:  //modulation
      for (byte i = 0; i < POLYPHONY; i++) LFO[i].setFreq_Q16n16((Q16n16) (val << 13 ));
      if (val == 0 && mod) mod = false;
      else if (val != 0 && !mod) mod = true;
      break;

    case 74: //volume
      volume = val;
      /*if (val == 0) {
        for (byte i = 0; i < POLYPHONY; i++) osc_is_on[i] = false;
        }*/
      break;

    case 71: //cutoff
      midi_cutoff = val;
      break;

    case 5: // pitchbend_amp
      pitchbend_amp = val;
      break;

  }
}



/**********************
      PITCHBEND
 *    ***************/
void HandlePitchBend(byte channel, int bend)
{
  pitchbend = bend;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (envelope[i].playing()) set_freq(i, false);
  }
}

/*********************
   AFTERTOUCH
 * *******************/

void HandleAfterTouchChannel(byte channel, byte after)
{
  aftertouch = after;
}


