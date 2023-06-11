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
	DWORD SafetyBytes;
	real32 tSine;
	int LatencySampleCount;
};

struct win32_debug_time_marker {
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD ExpectedFlipPlayCursor;
	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};


struct win32_game_code {
	
	//Either of the callbacks can be null, must check before calling, handmade.h  function declarations macros GAME_GET_UPDATE_RENDERER, GAME_GETSOUND_SAMPLES
	HMODULE GameCodeDLL;
	game_get_update_renderer *GameGetUpdateRenderer;
	game_get_sound_samples *GameGetSoundSamples;
	FILETIME DLLLastWriteTime;
	bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state {
	
	uint64 TotalSize;
	void *GameMemoryBlock;
	HANDLE RecordingHandle;
	int InputRecordingIndex;
	
	HANDLE PlayBackHandle;
	int InputPlayingIndex;
	
	char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
	char *OnePastLastEXEFileNameSlash;
};
#define WIN32_HANDMADE_H
#endif