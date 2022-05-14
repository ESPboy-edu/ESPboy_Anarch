/**
  @file main_espboy.ino

  This is ESPboy implementation of the game front end, using Arduino
  libraries.

  by Miloslav Ciz (drummyfish), 2021

  Sadly compiling can't be done with any other optimization flag than -Os.

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/

#define SFG_CAN_SAVE 1 // for version without saving, set this to 0

#define SFG_FOV_HORIZONTAL 240

#define SFG_SCREEN_RESOLUTION_X 128
#define SFG_SCREEN_RESOLUTION_Y 128
#define SFG_FPS 30
#define SFG_RAYCASTING_MAX_STEPS 50
#define SFG_RAYCASTING_SUBSAMPLE 2
#define SFG_DIMINISH_SPRITES 1
#define SFG_CAN_EXIT 0
#define SFG_DITHERED_SHADOW 1
/*
#define SFG_SCREEN_RESOLUTION_X 128
#define SFG_SCREEN_RESOLUTION_Y 128
#define SFG_FPS 30
#define SFG_RAYCASTING_MAX_STEPS 40
#define SFG_RAYCASTING_SUBSAMPLE 2
#define SFG_CAN_EXIT 0
#define SFG_DITHERED_SHADOW 1
#define SFG_RAYCASTING_MAX_HITS 10
#define SFG_ENABLE_FOG 0
#define SFG_DIMINISH_SPRITES 0
#define SFG_ELEMENT_DISTANCES_CHECKED_PER_FRAME 16
#define SFG_FOG_DIMINISH_STEP 2048
//#define SFG_DIFFERENT_FLOOR_CEILING_COLORS 1
*/

#define SFG_AVR 1

#include <Arduino.h>
#include <sigma_delta.h>

#include "lib/ESPboyInit.h"
#include "lib/ESPboyInit.cpp"


#if SFG_CAN_SAVE
#include <ESP_EEPROM.h>
#define SAVE_VALID_VALUE 73
EEPROMClass eeprom;
#endif

#include "game.h"

#define SOUND_PIN       D3
#define SAMPLE_RATE     8000

#include "sounds.h"


ESPboyInit myESPboy;

uint8_t *gamescreen;
uint8_t keys;

void SFG_setPixel(uint16_t x, uint16_t y, uint8_t colorIndex)
{
  gamescreen[y * SFG_SCREEN_RESOLUTION_X + x] = colorIndex;
}

void SFG_sleepMs(uint16_t timeMs)
{
}

uint32_t SFG_getTimeMs()
{
  return millis();
}

int8_t SFG_keyPressed(uint8_t key)
{
  switch (key)
  {
    case SFG_KEY_UP:    return keys & 0x02; break;
    case SFG_KEY_DOWN:  return keys & 0x04; break;
    case SFG_KEY_RIGHT: return keys & 0x08; break;
    case SFG_KEY_LEFT:  return keys & 0x01; break;
    case SFG_KEY_A:     return keys & 0x10; break;
    case SFG_KEY_B:     return keys & 0x20; break;
    case SFG_KEY_C:     return keys & 0x80; break;
    case SFG_KEY_MAP:   return keys & 0x40; break;
    default: return 0; break;
  }
}

void SFG_getMouseOffset(int16_t *x, int16_t *y)
{
}

int16_t activeSoundIndex = -1; //-1 means inactive
uint16_t activeSoundOffset = 0;
uint16_t activeSoundVolume = 0;

uint8_t musicOn = 0;

#define MUSIC_VOLUME   64

// ^ this has to be init to 0 (not 1), else a few samples get played at start

void ICACHE_RAM_ATTR audioFillCallback()
{
  int16_t s = 0;

  //SFG_musicTrackAverages should be not in program memory!
  if (musicOn) s = ((SFG_getNextMusicSample() - SFG_musicTrackAverages[SFG_MusicState.track]) & 0xffff) * MUSIC_VOLUME;

  if (activeSoundIndex >= 0)
  {
    s += (128 - SFG_GET_SFX_SAMPLE(activeSoundIndex, activeSoundOffset)) * activeSoundVolume;
    ++activeSoundOffset;
    if (activeSoundOffset >= SFG_SFX_SAMPLE_COUNT) activeSoundIndex = -1;
  }

  s = 128 + (s >> 5);
  if (s < 0) s = 0;
  if (s > 255) s = 255;

  sigmaDeltaWrite(0, s);
}

void SFG_setMusic(uint8_t value)
{
  switch (value)
  {
    case SFG_MUSIC_TURN_ON: musicOn = 1; break;
    case SFG_MUSIC_TURN_OFF: musicOn = 0; break;
    case SFG_MUSIC_NEXT: SFG_nextMusicTrack(); break;
    default: break;
  }
}

void SFG_save(uint8_t data[SFG_SAVE_SIZE])
{
#if SFG_CAN_SAVE
  eeprom.write(0, SAVE_VALID_VALUE);

  for (uint8_t i = 0; i < SFG_SAVE_SIZE; ++i)
    eeprom.write(i + 1, data[i]);

  eeprom.commit();
#endif
}

void SFG_processEvent(uint8_t event, uint8_t data)
{
}

uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE])
{
#if SFG_CAN_SAVE
  if (eeprom.read(0) == SAVE_VALID_VALUE)
    for (uint8_t i = 0; i < SFG_SAVE_SIZE; ++i)
      data[i] = eeprom.read(i + 1);

  return 1;
#else
  return 0;
#endif
}

void SFG_playSound(uint8_t soundIndex, uint8_t volume)
{
  activeSoundOffset = 0;
  activeSoundIndex = soundIndex;
  activeSoundVolume = 1 << (volume / 37);
}


void setup()
{
 myESPboy.begin("ANARCH 3D");
 gamescreen = new uint8_t [SFG_SCREEN_RESOLUTION_X * SFG_SCREEN_RESOLUTION_Y];
 

#if SFG_CAN_SAVE
  eeprom.begin(SFG_SAVE_SIZE + 1);
#endif

  SFG_init();


  //sound init

  //pinMode(SOUND_PIN, OUTPUT);

  sigmaDeltaSetup(0, 49000);
  sigmaDeltaAttachPin(SOUND_PIN);
  sigmaDeltaEnable();

  timer1_attachInterrupt(audioFillCallback);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
  timer1_write(80000000 / SAMPLE_RATE);
}


void loop(){
  keys = myESPboy.getKeys();
  SFG_mainLoopBody();

  static uint16_t scrbf[SFG_SCREEN_RESOLUTION_X];
  uint32_t readscrbf=0;
  
  for (uint32_t j=0; j<SFG_SCREEN_RESOLUTION_Y; j++){
   for (uint32_t i=0; i<SFG_SCREEN_RESOLUTION_X; i++)
     scrbf[i]=paletteRGB565[gamescreen[readscrbf++]];
   myESPboy.tft.pushPixels(&scrbf, SFG_SCREEN_RESOLUTION_X);
  }

//Print fps to Serial
/*
  static uint32_t tme;
  static uint8_t countr=0;
  countr++;
  if (countr>100){
    countr=0;
    Serial.println(100000/(millis() - tme));
    tme = millis();
  }
*/
}
