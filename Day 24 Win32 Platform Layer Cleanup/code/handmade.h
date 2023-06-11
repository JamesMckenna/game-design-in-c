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





//NOTE: HANDMADE_INTERNAL 0 - Build for public release, 1 - Build for developer only

//NOTE: HANDMADE_SLOW 0 - Build with no slow code, 1 - Slow code allowed
#include <math.h>
#include <stdint.h> 

#define internal static  
#define local_persist static 
#define global_variable static 
#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32; 

typedef float real32;
typedef double real64;

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)){ *(int *)0 = 0;}
#else
#define Assert(Expression)
#endif


#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32 SafeTrucateUInt64(uint64 Value){
		Assert(Value <= 0xFFFFFFFF);
		uint32 Result = (uint32)Value;
		return(Result);
}
/*
NOTE: Services that the platform provides to the game.
*/
#if HANDMADE_INTERNAL
struct debug_read_file_result {
	uint32 ContentSize;
	void *Contents;
};


#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

struct game_offscreen_buffer {
	void *Memory; 
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
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
	
	bool32 IsConnected;
	bool32 IsAnalog;
	real32 StickAverageX;
	real32 StickAverageY;
	
	union{
		game_button_state Buttons[13];
		struct{
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;
			
			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;
			
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
			
			game_button_state Start;
			game_button_state Back;
			
			//All buttons must be added above this comment
			game_button_state Terminator;
		};
	};
};

struct game_input {
	game_controller_input Controllers[5]; //1 Keyboard + Gamepads
};
inline game_controller_input *GetController(game_input *Input, int ControllerIndex){
	Assert(ControllerIndex <= ArrayCount(Input->Controllers));
	game_controller_input *Result = &Input->Controllers[ControllerIndex];
	return(Result);
}

struct game_state{
	int ToneHz;
	int GreenOffset;
	int BlueOffset;
	real32 tSine;
	int PlayerX;
	int PlayerY;
	real32 tJump;
};

struct game_memory{
	bool32 IsInitialized;
	uint64 PermanentStorageSize;
	void *PermanentStorage;
	uint64 TransientStorageSize;
	void * TransientStorage;
	
	debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
	debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
	debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
};


#define GAME_GET_UPDATE_RENDERER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_GET_UPDATE_RENDERER(game_get_update_renderer);
/*SEE:win32_game_code - Either of the callbacks can be null, must check before calling
GAME_GET_UPDATE_RENDERER(GameGetUpdateRendererStub)
{
}
*/


#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
/*SEE:win32_game_code - Because the stubs are now commented out (guarded?) either of the callbacks can be null, must check before calling
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}
*/


#define HANDMADE_H
#endif