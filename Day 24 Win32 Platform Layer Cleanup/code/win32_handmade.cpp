/*============================================================================================
$File: $
$Date: $
$Revision: $
$Creator: James McKenna $
$Notice (C) Copyright 2023 James McKenna All Rights Reserved. $
===============================================================================================*/

/*
This is not a final platform layer for windows. This code is about education; not Windows OS compatability
A partial list of stuff TODO to get to shipping state: 
Saved Game Locations (game state)
Getting a handle to our own executable file
Asset loading path
Threading (launching threads)
Raw Input (support for multiple keyboards)
Sleep/time begin period
Clip Cursor for multimonitor support
Fullscrenn support
WM_SETCUROSOR to controll cursor visibility
QueryCancellAutoplay
WM_ACTIVATEAPP for when not the active application
Blit speed imporvements (BitBlt)
Hardware acceleration (OpenGL and/or Direct3D)
*/

#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>
#include "win32_handmade.h"


#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub){ return(ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_get_state *OurXInputGetState = XInputGetStateStub;
#define XInputGetState OurXInputGetState

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return(ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_set_state *OurXInputSetState = XInputSetStateStub;
#define XInputSetState OurXInputSetState

internal int StringLength(char *String) {
	int Count = 0;
	while(*String++){
		++Count;
	}
	return(Count);
}

internal void CatStrings(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest){
		for(int Index = 0; Index < SourceACount; ++Index){
			*Dest++ = *SourceA++;
		}
		
		for(int Index = 0; Index < SourceBCount; ++Index){
			*Dest++ = *SourceB++;
		}
		*Dest++ = 0;
}

internal void Win32BuildEXEPathFileName(win32_state *Win32State, char *FileName, int DestCount, char *Dest){
	CatStrings(Win32State->OnePastLastEXEFileNameSlash - Win32State->EXEFileName, Win32State->EXEFileName, StringLength(FileName), FileName, DestCount, Dest);
}





//This will register the marco setup in handmade.h
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) { 
	if(Memory){
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile){
	
	debug_read_file_result Result = {};
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE){
		LARGE_INTEGER FileSize;
		if(GetFileSizeEx(FileHandle, &FileSize)) {
			uint32 FileSize32 = SafeTrucateUInt64(FileSize.QuadPart);//ReadFile can only read a file 32bit or less,
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if(Result.Contents){
				DWORD BytesRead;
				if(ReadFile(FileHandle, Result.Contents, (DWORD)FileSize.QuadPart, &BytesRead, 0) && (FileSize32 == BytesRead)){
					//SUCESS CASE so Do stuff
					Result.ContentSize = FileSize32;
				} 
				else {
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else {
				//TODO: Error logging
			}
		}
		else {
				//TODO Error logging
		}
		CloseHandle(FileHandle);
	}
	else {
			//TODO: Error logging
	}
	return(Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile){
	bool32 Result = false;
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE){
		DWORD BytesWritten;
		if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)){
			Result = (BytesWritten == MemorySize);
		} 
		else {
			//TODO: Error log
		}
		CloseHandle(FileHandle);
	}
	else {
			//TODO: Error logging
	}
	return(Result);	
}

inline FILETIME Win32GetLastWriteTime(char *SourceDLLName) {
	FILETIME LastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA Data;
	if(GetFileAttributesEx(SourceDLLName, GetFileExInfoStandard, &Data)){
		LastWriteTime = Data.ftLastWriteTime;
	}

	return(LastWriteTime);
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName){
	win32_game_code Result = {};
	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName); 
	
	CopyFile(SourceDLLName, TempDLLName, FALSE);
	Result.GameCodeDLL = LoadLibrary(TempDLLName);
	if(Result.GameCodeDLL) {
		Result.GameGetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
		Result.GameGetUpdateRenderer = (game_get_update_renderer *)GetProcAddress(Result.GameCodeDLL, "GameGetUpdateRenderer");
		
		Result.IsValid = (Result.GameGetUpdateRenderer && Result.GameGetSoundSamples);
	}
	
	if(!Result.IsValid){
		/*SEE:win32_game_code - Because the stubs are now commented out (guarded?) either of the callbacks can be null, must check before calling
			GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
			{
			}
		*/
			Result.GameGetUpdateRenderer = 0;
			Result.GameGetSoundSamples = 0;
	}
	return(Result);
}

internal void Win32UnloadGameCode(win32_game_code *GameCode){
	if(GameCode->GameCodeDLL) {
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->IsValid = false;
	GameCode->GameGetUpdateRenderer = 0;
	GameCode->GameGetSoundSamples = 0;
}

internal void Win32LoadXInput(void){
	HMODULE XInputLibrary = LoadLibrary("XInput9_1_0.dll");
	if(!XInputLibrary){
		XInputLibrary = LoadLibrary("XInput1_4.dll");
	}
	
	if(!XInputLibrary){
		XInputLibrary = LoadLibrary("XInput1_3.dll");
	}
	
	if(XInputLibrary){
			XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
			XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
	else {
		//TODO Diagnostic
	}
}

/* */
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);
/* */


global_variable bool32 Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;
global_variable bool32 GlobalPause = false;


internal void Win32ClearBuffer(win32_sound_output *SoundOutput){
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0))){ 
		uint8 *DestSample = (uint8 *)Region1;
		for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex){
		 *DestSample++ = 0;
		}
		
		DestSample = (uint8 *)Region2;
		for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex){
		 *DestSample++ = 0;
		}
		
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}	
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer){
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))){
	
		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex){
		 *DestSample++ = *SourceSample++;
		 *DestSample++ = *SourceSample++;
		 ++SoundOutput->RunningSampleIndex;
		}
		
		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex){
		 *DestSample++ = *SourceSample++;
		 *DestSample++ = *SourceSample++;
		 ++SoundOutput->RunningSampleIndex;
		}
		
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window){
	
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;
	
	return Result;
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height){
	
	//TODO: bullet proof this. Free up old buffer before overwritting. don't free first, then free first if that fails???
	//TODO: Free our DIBSection
	
	if(Buffer->Memory){
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	int BytesPerPixel = 4;
	Buffer->BytesPerPixel = BytesPerPixel;
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer *Buffer){
	//For prototyping purposes, we're going to alway Blit 1-to-1 pixels to make sure we don't introduce artifacts with stretching while we are learning to code the renderer.
	StretchDIBits(DeviceContext,
				0, 0, Buffer->Width, Buffer->Height,
				0, 0, Buffer->Width, Buffer->Height,
				Buffer->Memory, &Buffer->Info,
				DIB_RGB_COLORS, SRCCOPY);
}

internal void Win32InitDSound(HWND TheWindow, int32 SamplesPerSec, int32 BufferSize){
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	
	if(DSoundLibrary){
		//Get DirectSound Object (OOP)
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		
		
		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))){
			
			
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSec;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(TheWindow, DSSCL_PRIORITY))){
				//Create primary buffer
				DSBUFFERDESC BufferDescription = {}; 
				BufferDescription.dwSize = sizeof(BufferDescription); //Clears any existing value by initializing. clear to zero is a must
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))){
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))){ 
						//We have set the format
						OutputDebugStringA("Primary Buffer format was set\n");
					}
					else { 
						//TODO Diagnostic
					}
				}
				else { 
					//TODO Diagnostic
				}
			}
			else{
				//TODO Diagnostic
			}
			//Create secondary buffer
			DSBUFFERDESC BufferDescription = {}; 
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			//LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0))){
				//Start playing sound
				OutputDebugStringA("Secondary Buffer created\n");
			}
		}
		else {
			//TODO handle no direct sound, Diagnostic
		}
	}
}



internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState){
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
	
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown){
	Assert(NewState->EndedDown != IsDown);
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
	
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold){
	real32 Result = 0;
	if(Value < -DeadZoneThreshold){
		Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);	
	}
	else if(Value > DeadZoneThreshold){
		//Result = (real32)(Value - DeadZoneThreshold) / (32768.0f - DeadZoneThreshold); Thinking this case should be (Value + DeadZoneThreshold). Casey will probably catch this later
		Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
	}
	return(Result);
}


internal void Win32GetInputFileLocation(win32_state *Win32State, int SlotIndex, int DestCount, char *Dest) {
	Assert(SlotIndex == 1);
	Win32BuildEXEPathFileName(Win32State, "controller_loop_edit.hmi", DestCount, Dest);
}


internal void Win32BeginRecordingInput(win32_state *Win32State, int InputRecordingIndex){
	
	Win32State->InputRecordingIndex = InputRecordingIndex;
	char FileName[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(Win32State, InputRecordingIndex, sizeof(FileName), FileName);

	Win32State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	
	//Suppose to remove unneeded zero information from a file being written. Doesn't appear to speed up this file writing
	//DWORD Ignored;
	//DeviceIoCOntrol(Win32State->RecordingHandle, FSCTL_SET_SPARSE, 0, 0, 0, 0, &Ignored, 0);
	
	DWORD BytesToWrite = (DWORD)Win32State->TotalSize;
	Assert(Win32State->TotalSize == BytesToWrite);
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, Win32State->GameMemoryBlock, BytesToWrite , &BytesWritten, 0);
}

internal void Win32EndRecordingInput(win32_state *Win32State){
	CloseHandle(Win32State->RecordingHandle);
	Win32State->InputRecordingIndex = 0;
}

internal void Win32BeginInputPlayBack(win32_state *Win32State, int InputPlayingIndex){
	
	Win32State->InputPlayingIndex = InputPlayingIndex;
	char FileName[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(Win32State, InputPlayingIndex, sizeof(FileName), FileName);
	
	Win32State->PlayBackHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	DWORD BytesToRead = (DWORD)Win32State->TotalSize;
	Assert(Win32State->TotalSize == BytesToRead);
	DWORD BytesRead;
	ReadFile(Win32State->PlayBackHandle, Win32State->GameMemoryBlock, BytesToRead , &BytesRead, 0);
}

internal void Win32EndInputPlayBack(win32_state *Win32State){
	CloseHandle(Win32State->PlayBackHandle);
	Win32State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state *Win32State, game_input *NewInput){
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void Win32PlayBackInput(win32_state *Win32State, game_input *NewInput){
	DWORD BytesRead;
	if(ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0)) {
		if(BytesRead == 0){
			//File has been completely read in
			int PlayingIndex = Win32State->InputPlayingIndex;
			Win32EndInputPlayBack(Win32State);
			Win32BeginInputPlayBack(Win32State, PlayingIndex);
			ReadFile(Win32State->PlayBackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
		}
	}
}

internal void Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController){ 
	MSG Message;
	while(PeekMessageA(&Message,0,0,0, PM_REMOVE)){
		switch(Message.message){ 
			case WM_QUIT:{
				Running = false;
			}break;
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:{
				uint32 VKCode = (uint32)Message.wParam;
				//Since we are comparing WasDown to IsDown, we must use the == and != to convert these bit tests to actual 0 or 1 values.
				bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
				bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
				if(WasDown != IsDown){
					if(VKCode == 'W'){
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if(VKCode == 'A'){
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if(VKCode == 'S'){
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if(VKCode == 'D'){
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if(VKCode == 'Q'){
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if(VKCode == 'E'){
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if(VKCode == VK_UP){
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if(VKCode == VK_DOWN){
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if(VKCode == VK_LEFT){
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if(VKCode == VK_RIGHT){
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if(VKCode == VK_ESCAPE){
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if(VKCode == VK_SPACE){
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}			
#if HANDMADE_INTERNAL
					else if(VKCode == 'P'){
						if(IsDown){
							GlobalPause = !GlobalPause;
						}
					}
					else if(VKCode == 'L') {
						if(IsDown){
							if(Win32State->InputRecordingIndex == 0) {
								Win32BeginRecordingInput(Win32State, 1);
							}
							else{
								Win32EndRecordingInput(Win32State);
								Win32BeginInputPlayBack(Win32State, 1);
							}
						}
							
					}
#endif
					bool32 AltKeyIsDown = (Message.lParam & (1 << 29));
					if((VKCode == VK_F4) && AltKeyIsDown){
						Running = false;
					}
				}
			}break;
			default:{
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			}break;
		}
	}
}


//Main Window Callback
LRESULT Wndproc(HWND WindowToHandle, UINT MessageToWindow, WPARAM WParam, LPARAM LParam){
	LRESULT Result = 0;

	switch(MessageToWindow){
		
		case WM_SIZE:
		{

		} break;
		case WM_DESTROY:
		{
			Running = false;
		} break;
		case WM_CLOSE:
		{
			Running = false;
		} break;
		case WM_ACTIVATEAPP:
		{
#if 0
			if(WParam == TRUE) {
				SetLayeredWindowAttributes(WindowToHandle, RGB(0,0,0), 255, LWA_ALPHA);
			}
			else {
				SetLayeredWindowAttributes(WindowToHandle, RGB(0,0,0), 64, LWA_ALPHA);
			}
#endif
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in through a non-dispatch message");
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT Paint; 
			HDC DeviceContext = BeginPaint(WindowToHandle, &Paint);
			win32_window_dimension Dimension = Win32GetWindowDimension(WindowToHandle);	
			Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
			EndPaint(WindowToHandle, &Paint);
		}break;
		default:
		{
			OutputDebugStringA("DEFAULT CASE\n");
			Result = DefWindowProcA(WindowToHandle, MessageToWindow, WParam, LParam);
		} break;
	}
	return Result;
}

inline LARGE_INTEGER Win32GetWallClock(void) { 
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}
						
inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End){
	real32 Result = ((real32)End.QuadPart - Start.QuadPart / (real32)GlobalPerfCounterFrequency);
	return(Result);
}



internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer, int X, int Top, int Bottom, uint32 Color){

	if(Top <= 0){
		Top = 0;
	}

	if(Bottom > BackBuffer->Height) {
		Bottom = BackBuffer->Height;
	}

	if((X >= 0) && (X < BackBuffer->Width)){	
		uint8 *Pixel = ((uint8 *)BackBuffer->Memory + X*BackBuffer->BytesPerPixel + Top*BackBuffer->Pitch);
		for(int Y = Top; Y < Bottom; ++Y){
			*(uint32 *)Pixel = Color;
			Pixel += BackBuffer->Pitch;
		}
	}
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer, win32_sound_output *SoundOutput, real32 C, int PadX, int Top, int Bottom, DWORD Value, uint32 Color) {
	real32 xReal32 = (C * (real32)Value);
	int X = PadX +(int)xReal32;
	Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer, int MarkerCount, win32_debug_time_marker *Markers, int CurrentMarkerIndex, win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame){
	
	int PadX = 16;
	int PadY = 16;
	int LineHeight = 64;
	
	real32 C = (real32)(BackBuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
	for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex){
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);
		
		int Top = PadY;
		int Bottom = PadY + LineHeight;
		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;
		
		if(MarkerIndex == CurrentMarkerIndex) {
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			int FirstTop = Top;

			
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);
			
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);
			
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
		}
		
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + (480*SoundOutput->BytesPerSample), PlayWindowColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
	}
}




internal void Win32GetEXEFileName(win32_state *Win32State) {
	DWORD SizeOfFilename = GetModuleFileNameA(0, Win32State->EXEFileName, sizeof(Win32State->EXEFileName));
	Win32State->OnePastLastEXEFileNameSlash = Win32State->EXEFileName;
	for(char *Scan = Win32State->EXEFileName; *Scan; ++Scan){
		if(*Scan == '\\'){
			Win32State->OnePastLastEXEFileNameSlash = Scan + 1;
		}
	}
}

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
	{ 	
		win32_state Win32State = {};
		
		LARGE_INTEGER PerfCounterFequencyResult;
		QueryPerformanceCounter(&PerfCounterFequencyResult);
		GlobalPerfCounterFrequency = PerfCounterFequencyResult.QuadPart;
		
		Win32GetEXEFileName(&Win32State);
	
		char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
		Win32BuildEXEPathFileName(&Win32State, "handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
		
		char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
		Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll", sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

		//NOTE: Set Windows Scheduler granularity to 1ms so the Sleep() can be more granular
		UINT DesiredSchedulerMS = 1;
		bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

		Win32LoadXInput();
		WNDCLASSA WindowClass = {};
		
		Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
		
		WindowClass.style = CS_HREDRAW|CS_VREDRAW;
		WindowClass.lpfnWndProc = (WNDPROC)Wndproc;
		WindowClass.hInstance = hInstance;
		//WindowClass.hIcon;
		WindowClass.lpszClassName = "HandmadeHeroWindowClass";

#define MonitorRefreshHz 60
#define	GameUpdateHz (MonitorRefreshHz / 2)
		real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

		if(RegisterClassA(&WindowClass))
		{
			HWND TheWindow = CreateWindowExA(
				0,//WS_EX_TOPMOST|WS_EX_LAYERED,
				WindowClass.lpszClassName,
				"Handemade Hero",
				WS_OVERLAPPEDWINDOW|WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				WindowClass.hInstance,
				0);
				
			if(TheWindow){
				win32_sound_output SoundOutput = {};
				
				SoundOutput.SamplesPerSecond = 48000;
				SoundOutput.RunningSampleIndex = 0;
				SoundOutput.BytesPerSample = sizeof(int16)*2;
				SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
				SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / GameUpdateHz); 
				SoundOutput.SafetyBytes = ((SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample / GameUpdateHz) / 3);
				
				Win32InitDSound(TheWindow, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
				Win32ClearBuffer(&SoundOutput);
				GlobalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING);
				
				Running = true;
#if 0
				while(Running){
					
					LARGE_INTEGER BeginCounter;
					QueryPerformanceCounter(&BeginCounter);
					BeginCounter.QuadPart

					DWORD PlayCursor;
					DWORD WriteCursor;
					GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
					
					char TextBuffer[256];
					_snprintf_s(TextBuffer, sizeof(TextBuffer), "PC:%u, WC:%u\n", PlayCursor, WriteCursor);
					OutputDebugStringA(TextBuffer);
				}
#endif	
				
				//TODO: Pool all VirualAlloc together (Bitmap and Sound)
				int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize*2, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);				

#if HANDMADE_INTERNAL
	LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
	LPVOID BaseAddress = 0;
#endif

				game_memory GameMemory = {};
				GameMemory.PermanentStorageSize = Megabytes(64);
				GameMemory.TransientStorageSize = Gigabytes((uint64)1);

				GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
				GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
				GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
				
				Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
				Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
				GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
				GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
				
				if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage){
				
					game_input Input[2] = {};
					game_input *NewInput = &Input[0];
					game_input *OldInput = &Input[1];
				
					LARGE_INTEGER LastCounter = Win32GetWallClock();
					LARGE_INTEGER FlipWallClock = Win32GetWallClock();
					
					int DebugTimeMarkerIndex = 0;
					win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};
					
					DWORD AudioLatencyBytes = 0;
					real32 AudioLatencySeconds = 0;
					bool32 SoundIsValid = false;
					
					win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
					uint32 LoadCounter = 0;
					
					int64 LastCycleCount = __rdtsc();
					while(Running){
						
						FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
						if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0) {
							Win32UnloadGameCode(&Game);
							Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
							LoadCounter = 0;
						}
						
						game_controller_input *OldKeyboardController = GetController(OldInput, 0);
						game_controller_input *NewKeyboardController = GetController(NewInput, 1);
						*NewKeyboardController = {};
						NewKeyboardController->IsConnected = true;
						for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex){
							NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
						}
						Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
						
						DWORD MaxControllerCount = XUSER_MAX_COUNT;
						if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)){
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}
						for(DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex){
							DWORD OurControllerIndex = ControllerIndex +1;
							game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
							
							XINPUT_STATE ControllerState;
							if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS){
								
								NewController->IsConnected = true;
								NewController->IsAnalog = OldController->IsAnalog;
								XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

								
								NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								
								if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f)){
									NewController->IsAnalog = true;
								}
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP){
									NewController->StickAverageY = 1.0f;
									NewController->IsAnalog = false;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN){
									NewController->StickAverageY = -1.0f;
									NewController->IsAnalog = false;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT){
									NewController->StickAverageX = -1.0f;
									NewController->IsAnalog = false;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT){
									NewController->StickAverageX = 1.0f;
									NewController->IsAnalog = false;
								}
								
								real32 Threshold = 0.5f;
								DWORD FakeMoveButtons = (NewController->StickAverageX < -Threshold) ? 1 : 0;
								Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft, 1, &NewController->MoveLeft);
								Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, &OldController->MoveRight, 1, &NewController->MoveRight);
								Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown, 1, &NewController->MoveDown);
								Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, &OldController->MoveUp, 1, &NewController->MoveUp);
								
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
								
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
								Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
							}
							else{
									NewKeyboardController->IsConnected = false;
							}
						}
									
						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBuffer.Memory;
						Buffer.Width = GlobalBackBuffer.Width;
						Buffer.Height = GlobalBackBuffer.Height;
						Buffer.Pitch = GlobalBackBuffer.Pitch;
						Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
						
						if(Win32State.InputRecordingIndex){
							Win32RecordInput(&Win32State, NewInput);
						}
						if(Win32State.InputPlayingIndex){
							Win32PlayBackInput(&Win32State, NewInput);
						}
						
						/*SEE:win32_game_code - Because the stubs are now commented out (guarded?) either of the callbacks can be null, must check before calling
							GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
							{
							}
						*/
						if(Game.GameGetUpdateRenderer){
							Game.GameGetUpdateRenderer(&GameMemory, NewInput, &Buffer);
						}

						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);


						DWORD PlayCursor;
						DWORD WriteCursor;
						if((GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)) == DS_OK) { 
							/*
								Sound Output Computation
							
								We define a saftley value that is the number of samples we think our game update loop may vary by.
							
								When we wake up to write audio, we will look and see what the play cursor position is and we will forecast ahead where we think the play cursor will be on the next frame boudry.
							
								We will then look to see if the write cursor is before that by our saftey value. If it is, the target fill position is that frame boundry plus 1 frame - giving us perfect(?) audio sync in the case that the card has low enough latency.
							
								If the write cursor is after the saftey value, then we assume we can never sync audio perfectly, so we will write 1 frame's worth of audio plus the safety value worth of guard samples.
							*/

							if(!SoundIsValid){
									SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
									SoundIsValid = true;
							}
							
							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
							
							DWORD ExpectedSoundBytesPerFrame = ((SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) / GameUpdateHz);
							real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntillFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);
							
							
							DWORD ExpectedFrameBoundryByte = PlayCursor + ExpectedBytesUntillFlip;
							DWORD SafeWriteCursor = WriteCursor;
							if(SafeWriteCursor < PlayCursor) { 
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;
							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundryByte);

							DWORD TargetCursor = 0;
							if(AudioCardIsLowLatency) {
								TargetCursor = (ExpectedFrameBoundryByte + ExpectedSoundBytesPerFrame);
							}
							else {
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
							}
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);
							
							DWORD BytesToWrite = 0;
							if(ByteToLock > TargetCursor){
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							
							/* SEE:win32_game_code - Because the stubs are now commented out (guarded?) either of the callbacks can be null, must check before calling
								GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
								{
								}
							*/
							if(Game.GameGetSoundSamples) {
								Game.GameGetSoundSamples(&GameMemory, &SoundBuffer);
							}
							
#if HANDMADE_INTERNAL
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if(UnwrappedWriteCursor < PlayCursor){ 
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
							
							char TextBuffer[256];
							_snprintf_s(TextBuffer, sizeof(TextBuffer), "BTL:%u, TC:%u, BTW:%u, PC:%u WC:%u, BBC:%u, BBCinS:%f\n",  ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
							OutputDebugStringA(TextBuffer);
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
						}
						else {
							SoundIsValid = false;
						}
												
						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
							
						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if(SecondsElapsedForFrame < TargetSecondsPerFrame){
							if(SleepIsGranular){
								DWORD SleepMS = (DWORD)(1000.0f*(TargetSecondsPerFrame - SecondsElapsedForFrame));
								if(SleepMS > 0){
									Sleep(SleepMS);
								}
							}
							
							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							if(TestSecondsElapsedForFrame < TargetSecondsPerFrame){
								//Log missed sleep
							}
							
							while(SecondsElapsedForFrame < TargetSecondsPerFrame){
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else {
								//TODO: Missed Frame Rate
								//TODO: log it!!!
						}
						
						LARGE_INTEGER EndCounter = Win32GetWallClock();
						EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastCounter, EndCounter);
						//real32 MSPerFrame = Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;
						
						win32_window_dimension Dimension = Win32GetWindowDimension(TheWindow);
#if HANDMADE_INTERNAL
						//This is wrong on the zero index
						Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, DebugTimeMarkerIndex - 1, &SoundOutput, TargetSecondsPerFrame);
#endif
						
						HDC DeviceContext = GetDC(TheWindow);
						Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
						ReleaseDC(TheWindow, DeviceContext);
						
						FlipWallClock = Win32GetWallClock();



#if HANDMADE_INTERNAL
					{
						//DWORD PlayCursor;
						//DWORD WriteCursor;
						if((GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)) == DS_OK) { 
							Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->FlipPlayCursor = PlayCursor;
							Marker->FlipWriteCursor = WriteCursor;
						}
					}
#endif
		
						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
						

						
				
						int64 EndCycleCount = __rdtsc();
						/*************************************/
						//LARGE_INTEGER EndCounter;
						QueryPerformanceCounter(&EndCounter);
					
						//uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						//int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
						//real32 MSPerFrame = (((1000.0f*(real32)CounterElapsed) / (real32)GlobalPerfCounterFrequency));
						/****************************************/
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;
	
						int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
						real32 FPS = (real32)GlobalPerfCounterFrequency / (real32)CounterElapsed;
						
						real32 MegaCyclesPerFrame = ((real32)CyclesElapsed/(1000.0f*1000.0f));

						char FPSBuffer[256];
						_snprintf_s(FPSBuffer, sizeof(FPSBuffer), "Milliseconds/frame %f ms / %f FPS %f MegaCycles/frame\n", MSPerFrame, FPS, MegaCyclesPerFrame);
						OutputDebugStringA(FPSBuffer);
#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if(DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)){
								DebugTimeMarkerIndex = 0;
						}
#endif
					}
				}
				else{
					//TODO: Samples && GameMemory.PermanentStorage error
				}
			}
			else{
				//TODO: Log CreateWindowExA error
			}
		}
		else{
			//TODO: Log that wie WindowClass wasn't registered
		}

		return 0; 
	}    