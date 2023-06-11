/*============================================================================================
$File: $
$Date: $
$Revision: $
$Creator: James McKenna $
$Notice (C) Copyright 2023 James McKenna All Rights Reserved. $
===============================================================================================*/
#include "handmade.h"





		//DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		//int16 *SampleOut = (int16 *)Region1;



internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz){
	
	local_persist real32 tSine;
	int16 ToneVolume = 3000;	
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;
	
	int16 *SampleOut = SoundBuffer->Samples;
	
	for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex){

		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;
	}
}


internal void RenderWierdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset){
	uint8 *Row = (uint8 *)Buffer->Memory;
	for(int Y = 0; Y < Buffer->Height; ++Y){
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; X < Buffer->Width; ++X){
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}


internal void GameUpdateRenderer(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset, game_sound_output_buffer *SoundBuffer, int ToneHz){
	
	//TODO: allow sample offsets here for more robust platform options
	GameOutputSound(SoundBuffer, ToneHz);
	RenderWierdGradient(Buffer, BlueOffset, GreenOffset);
}
