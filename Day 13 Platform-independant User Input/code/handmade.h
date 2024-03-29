#if !defined(HANDMADE_H)
/*============================================================================================
$File: $
$Date: $
$Revision: $
$Creator: James McKenna $
$Notice (C) Copyright 2023 James McKenna All Rights Reserved. $
===============================================================================================*/

/*
NOTE: Services that the game provides to the platform layer
*/
//Requires timing, controller/keyboard input to use, bitmap buffer top use & sound buffer to to use



/*
NOTE: Services that the platform provides to the game.
*/

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))


struct game_offscreen_buffer {
	void *Memory; 
	int Width;
	int Height;
	int Pitch;
};

struct game_sound_output_buffer{
	
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
};

struct game_button_state{
	int HalfTransitionCount;
	bool32 EndedDown;
};

struct game_controller_input{
	
	bool32 IsAnalog;
	
	real32 StartX;
	real32 StartY;
	real32 MinX;
	real32 MinY;
	real32 MaxX;
	real32 MaxY;
	real32 EndX;
	real32 EndY;
	
	union{
		game_button_state Buttons[6];
		struct{
			game_button_state Up;
			game_button_state Down;
			game_button_state Left;
			game_button_state Right;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
		};
	};
};

struct game_input {
	game_controller_input Controllers[4];
};


internal void GameUpdateRenderer(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif