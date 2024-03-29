#if !defined(WIN32_HANDMADE_H)

/*============================================================================================
$File: $
$Date: $
$Revision: $
$Creator: James McKenna $
$Notice (C) Copyright 2023 James McKenna All Rights Reserved. $
===============================================================================================*/


struct win32_offscreen_buffer {
	BITMAPINFO Info;
	void *Memory; 
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

struct win32_sound_output {
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	DWORD SecondaryBufferSize;
	real32 tSine;
	int LatencySampleCount;
};

struct win32_debug_time_marker {
	DWORD PlayCursor;
	DWORD WriteCursor;
};

#define WIN32_HANDMADE_H
#endif