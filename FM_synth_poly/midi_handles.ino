#include "midi_handles.h"



/*********************
       NOTE ON
 ********************/
void HandleNoteOn(byte channel, byte note, byte velocity)
{
  byte min_rank = 255;
  int empty_arg = -1;
  for (byte i = 0; i < POLYPHONY; i++)  //take a non playing oscil
  {
    if (!envelope[i].playing() && oscil_rank[i] < min_rank)
    {
      empty_arg = i;
      min_rank = oscil_rank[i];
      //break;
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


  if (empty_arg != -1)   //an empty oscil has been found
  {
    notes[empty_arg] = note;
    set_freq(empty_arg);
    //digitalWrite(LED, HIGH);
    envelope[empty_arg].setADLevels(velocity, velocity);
    envelope[empty_arg].noteOn();
    oscil_state[empty_arg] = 1;
    byte max_rank = 0;


    for (byte i = 0; i < POLYPHONY; i++)
    {
      if (oscil_rank[i] > max_rank) max_rank = oscil_rank[i];
    }
    oscil_rank[empty_arg] = max_rank + 1;
    //set_freq(empty_arg);
  }


  else  // no empty oscil, kill one in sustain mode! (the oldest)
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

    notes[min_rank_arg] = note;
    set_freq(empty_arg);
    envelope[min_rank_arg].setADLevels(velocity, velocity);
    envelope[min_rank_arg].noteOn();
    oscil_state[min_rank_arg] = 1;
    
    //for (byte i = 0; i < POLYPHONY; i++) oscil_rank[i] -= min_rank;


    byte max_rank = 0;
    for (byte i = 0; i < POLYPHONY; i++)
    {
      if (oscil_rank[i] > max_rank) max_rank = oscil_rank[i];
    }
    oscil_rank[min_rank_arg] = max_rank + 1;
    //Serial.println(max_rank);

  }

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


}


/**********************
      NOTE OFF
 *********************/


void HandleNoteOff(byte channel, byte note, byte velocity)
{
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
    if (!sustain)
    {
      envelope[to_kill].noteOff();
      oscil_state[to_kill] = 0;
    }
    else oscil_state[to_kill] = 2;
    digitalWrite(LED, LOW);
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
      {
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
        else
        {
          sustain = true;
          if (millis() - last_sustain_time < 500)
          {
            for (byte i = 0; i < POLYPHONY; i++)
            {
              envelope[i].noteOff();
              oscil_state[i] = 0;
            }
          }
          last_sustain_time = millis();

        }
        break;
      }
      case 1:  //modulation W
      {
        modulationW = val;
        break;
      }
  }
}


/**********************
 *    PITCHBEND
 *    ***************/
 void HandlePitchBend(byte channel, int bend)
 {
  pitchbend = bend;
  for (byte i = 0; i < POLYPHONY; i++)
  {
    if (envelope[i].playing()) set_freq(i);
  }
 }


 /**********************
  * AFTERTOUCH
 ******************** */

 void HandleAfterTouchChannel(byte channel, byte after)
 {
  aftertouch = after;
 }

