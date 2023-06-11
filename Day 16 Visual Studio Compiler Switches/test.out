/*============================================================================================
$File: $
$Date: $
$Revision: $
$Creator: James McKenna $
$Notice (C) Copyright 2023 James McKenna All Rights Reserved. $
===============================================================================================*/
#include "handmade.h"

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
			uint8 Blue = (uint8)(X + BlueOffset);
			uint8 Green = (uint8)(Y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}


internal void GameUpdateRenderer(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer){
	
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	
	char *Filename = __FILE__;
	debug_read_file_result File = DEBUGPlatformReadEntireFile(Filename);
	if(File.Contents){
		DEBUGPlatformWriteEntireFile("../test.out", File.ContentSize, File.Contents);
		DEBUGPlatformFreefileMemory(File.Contents);
	}
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if(!Memory->IsInitialized){
		GameState->ToneHz = 256;
	
		//NOTE: It may be better to let platform layer initialize this
		Memory->IsInitialized = true;
	}
	
	game_controller_input *Input0 = &Input->Controllers[0];
	
	if(Input0->IsAnalog){
		//NOTE: Use analog movement tuning, meaning using the joystick (joystick is an analog device???)
		GameState->BlueOffset += (int)(4.0f*(Input0->EndX));
		GameState->ToneHz = 256 + (int)(128.0f*(Input0->EndY));
	}
	else {
		//NOTE: Use digital movement tuning, meaning using the keyboard. 
	}
	
	
	if(Input0->Down.EndedDown){
		GameState->GreenOffset += 1;
	}
	//Input.ABtnHalfTranistionCount;
	

	
	Input0->StartX;
	Input0->EndX;
	Input0->MinX;
	Input0->MaxX;

	Input0->StartY;
	Input0->EndY;
	Input0->MinY;
	Input0->MaxY;
	
	
	//TODO: allow sample offsets here for more robust platform options
	GameOutputSound(SoundBuffer, GameState->ToneHz);
	RenderWierdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}
