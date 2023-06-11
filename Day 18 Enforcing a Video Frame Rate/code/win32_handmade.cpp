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

//TODO: implement sin ourselves
#include <math.h>
#include "handmade.cpp"


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

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename){
	
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
					DEBUGPlatformFreefileMemory(Result.Contents);
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
internal void DEBUGPlatformFreefileMemory(void *Memory) { 
	if(Memory){
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory){
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
/* GAMEPAD DLL MISSING GUARD ENDS HERE */


/* */
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);
/* */


global_variable bool32 Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;


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
	//TODO: Aspect Ratioo correction needed.

	StretchDIBits(DeviceContext,
				0, 0, WindowWidth, WindowHeight,
				0, 0, Buffer->Width, Buffer->Height,
				Buffer->Memory, &Buffer->Info,
				DIB_RGB_COLORS, SRCCOPY);
}

internal void Win32InitDSound(HWND TheWindow, int32 SamplesPerSec, int32 BufferSize){
	//https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416960(v=vs.85)
	//Load DirectSound Library
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
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8; //divide by number of bits in a byte
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
			
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(TheWindow, DSSCL_PRIORITY))){
				//Create primary buffer
				DSBUFFERDESC BufferDescription = {}; 
				BufferDescription.dwSize = sizeof(BufferDescription); //Clesars any existing value by initializing. clear to zero is a must
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
			BufferDescription.dwFlags = 0;
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

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController){ 
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
				int32 VKCode = (uint32)Message.wParam;
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
			//OutputDebugStringA("WM_DESTROY\n");
			//TODO: Handle as error message, maybe recreate window???
			Running = false;
		} break;
		case WM_CLOSE:
		{
			//OutputDebugStringA("WM_CLOSE\n");
			//TODO: handle with confirm message to user.
			Running = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
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

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
	{ 
	

	
		LARGE_INTEGER PerfCounterFequencyResult;
		QueryPerformanceCounter(&PerfCounterFequencyResult);
		GlobalPerfCounterFrequency = PerfCounterFequencyResult.QuadPart;
		
		
		//NOTE: Set Windows Scheduler granularity to 1ms so the Sleep() can be more granular
		UINT DesiredSchedulerMS = 1;
		bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

		Win32LoadXInput();
		WNDCLASSA WindowClass = {};
		
		Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
		
		WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
		WindowClass.lpfnWndProc = (WNDPROC)Wndproc;
		WindowClass.hInstance = hInstance;
		//WindowClass.hIcon;
		WindowClass.lpszClassName = "HandmadeHeroWindowClass";

		int MonitorRefreshHz = 60;
		int GameUpdateHz = MonitorRefreshHz / 2;
		real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

		if(RegisterClassA(&WindowClass))
		{
			HWND TheWindow = CreateWindowExA(
				0,
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
				
				HDC DeviceContext = GetDC(TheWindow);
				
				Running = true;

				
			
				win32_sound_output SoundOutput = {};
				
				SoundOutput.SamplesPerSecond = 48000;
				SoundOutput.RunningSampleIndex = 0;
				SoundOutput.BytesPerSample = sizeof(int16)*2;
				SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
				SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15; 
				
				Win32InitDSound(TheWindow, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
				Win32ClearBuffer(&SoundOutput);
				GlobalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING);
				
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
				uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
				GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
				GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
															
				
				if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage){
				
					game_input Input[2] = {};
					game_input *NewInput = &Input[0];
					game_input *OldInput = &Input[1];
					

					LARGE_INTEGER LastCounter = Win32GetWallClock();
					int64 LastCycleCount = __rdtsc();
			
					while(Running){
						
						game_controller_input *OldKeyboardController = GetController(OldInput, 0);
						game_controller_input *NewKeyboardController = GetController(NewInput, 0);
						*NewKeyboardController = {};
						NewKeyboardController->IsConnected = true;
						for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex){
							NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
						}
						Win32ProcessPendingMessages(NewKeyboardController);
						
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
						
						
						//https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708932(v=vs.85)
						DWORD PlayCursor = 0;
						DWORD WriteCursor = 0;
						DWORD ByteToLock = 0;
						DWORD BytesToWrite = 0;
						DWORD TargetCursor = 0;
						bool32 SoundIsValid = false;
						//TODO: TIghten up sound logic so we know where we should be writing to and can antincipate the time spent in the game update
						if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))){
							ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
							TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize);
							
							if(ByteToLock > TargetCursor){
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else{
								BytesToWrite = TargetCursor - ByteToLock;
							}
							SoundIsValid = true;
						}
						
						//int16 *Samples = (int16 *)_alloca(48000* 2*sizeof(int16));
						game_sound_output_buffer SoundBuffer = {};
						SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
						SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
						SoundBuffer.Samples = Samples;
						
						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBuffer.Memory;
						Buffer.Width = GlobalBackBuffer.Width;
						Buffer.Height = GlobalBackBuffer.Height;
						Buffer.Pitch = GlobalBackBuffer.Pitch;
						
						GameUpdateRenderer(&GameMemory, NewInput, &Buffer,&SoundBuffer);
						
						if(SoundIsValid){
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
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
							Assert(TestSecondsElapsedForFrame < TargetSecondsPerFrame);
							
							while(SecondsElapsedForFrame < TargetSecondsPerFrame){
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else {
								//TODO: Missed Frame Rate
								//TODO: log it!!!
						}
						
						win32_window_dimension Dimension = Win32GetWindowDimension(TheWindow);
						Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
						ReleaseDC(TheWindow, DeviceContext); //Was this moved or deleted?? Or not needed because we only create 1 window??

		
						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
						
						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;
						
				
						int64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;
	
						real32 FPS = 0.0f;
						real32 MegaCyclesPerFrame = ((real32)CyclesElapsed/(1000.0f*1000.0f));

						char FPSBuffer[256];
						//C Stanrd Library (stdio.h sprintf) & Windows
						_snprintf_s(FPSBuffer, sizeof(FPSBuffer), "Milliseconds/frame %f ms / %f FPS %f MegaCycles/frame\n", MSPerFrame, FPS, MegaCyclesPerFrame);
						OutputDebugStringA(FPSBuffer);//My value differ drastically from Casey's. Casey was using OBS at the same time. My processor is 8 years newer but I was getting .00008 FPS when Casey was getting 8FPS?????? Did my machine use the powerful NVidia GPU?

						

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