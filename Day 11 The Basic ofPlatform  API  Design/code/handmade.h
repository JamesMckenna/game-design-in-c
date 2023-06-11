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
struct game_offscreen_buffer {
	void *Memory; 
	int Width;
	int Height;
	int Pitch;
};

internal void GameUpdateRenderer(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

#define HANDMADE_H
#endif