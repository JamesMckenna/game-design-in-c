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

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit, game_button_state *NewState){
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
	
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
			int32 VKCode = WParam;
			bool32 WasDown = ((LParam & (1 << 30)) != 0);
			bool32 IsDown = ((LParam & (1 << 31)) == 0);
			bool32 AltKeyIsDown = LParam & (1 << 29);
			
			if(VKCode == 'W'){
				OutputDebugStringA("W ");
				if(IsDown){
					OutputDebugStringA("is down\n");
				}
				if(WasDown){
					OutputDebugStringA("was down\n");
				}
			}
			else if(VKCode == 'A'){
				OutputDebugStringA("A\n");
			}
			else if(VKCode == 'S'){
				OutputDebugStringA("S\n");
			}
			else if(VKCode == 'D'){
				OutputDebugStringA("D\n");
			}
			else if(VKCode == 'Q'){
				OutputDebugStringA("Q\n");
			}
			else if(VKCode == 'E'){
				OutputDebugStringA("E\n");
			}
			else if(VKCode == VK_UP){
				OutputDebugStringA("up arrow\n");
			}
			else if(VKCode == VK_DOWN){
				OutputDebugStringA("down arrow\n");
			}
			else if(VKCode == VK_LEFT){
				OutputDebugStringA("left arrow\n");
			}
			else if(VKCode == VK_RIGHT){
				OutputDebugStringA("right arrow\n");
			}
			else if(VKCode == VK_ESCAPE){
				OutputDebugStringA("escape key\n");
			}
			else if(VKCode == VK_SPACE){
				OutputDebugStringA("spacebar\n");
			}			
			else if((VKCode == VK_F4) && AltKeyIsDown){
				Running = false;//NOT SURE IF THIS ELSE IF IS PROPERLY SET...NOT IN CORRECT PLACE, OWN IF STATEMENT MIGHT BE NEEDED
			}
			else{
				OutputDebugStringA("else, key not handled\n");
			}
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

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
	{ 
	
	
		LARGE_INTEGER PerfCounterFequencyResult;
		QueryPerformanceCounter(&PerfCounterFequencyResult);
		int64 PerfCounterFrequency = PerfCounterFequencyResult.QuadPart;
	
		Win32LoadXInput();
		WNDCLASS WindowClass = {};
		
		Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
		
		WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
		WindowClass.lpfnWndProc = Wndproc;
		WindowClass.hInstance = hInstance;
		//WindowClass.hIcon;
		WindowClass.lpszClassName = "HandmadeHeroWindowClass";

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

				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];



				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
				//https://learn.microsoft.com/en-us/cpp/intrinsics/rdtsc?view=msvc-170
				int64 LastCycleCount = __rdtsc();
				
				while(Running){
					
					//https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter
					// LARGE_INTEGER BeginCounter;
					// QueryPerformanceCounter(&BeginCounter);
					// BeginCounter.QuadPart
					
					MSG Message;
					
					while(PeekMessageA(&Message,0,0,0, PM_REMOVE)){
						
						if(Message.message == WM_QUIT){
							Running = false;
						}
						
						TranslateMessage(&Message);
						DispatchMessageA(&Message);
					}
					
					int MaxControllerCount = XUSER_MAX_COUNT;
					if(MaxControllerCount > ArrayCount(NewInput->Controllers)){
						MaxControllerCount = ArrayCount(NewInput->Controllers);
					}
					for(DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex){
						
						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];
						
						XINPUT_STATE ControllerState;
						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS){
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
							
							bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
							
							NewController->IsAnalog = true;
							NewController->StartX = OldController->EndX;
							NewController->StartY = OldController->EndY;						
							real32 X;
							if(Pad->sThumbLX < 0){
								X = (real32)Pad->sThumbLX / 32768.0f;	
							}
							else{
								X = (real32)Pad->sThumbLX / 32767.0f;	
							}
							NewController->MinX = NewController->MaxX = NewController->EndX = X;
							
							real32 Y;
							if(Pad->sThumbLY < 0){
								Y = (real32)Pad->sThumbLY / 32768.0f;	
							}
							else{
								Y = (real32)Pad->sThumbLY / 32767.0f;	
							}
							NewController->MinY = NewController->MaxY = NewController->EndY = Y;
							
							
							
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
							
							//Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->State, XINPUT_GAMEPAD_START, NewController->State);
							//Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->State, XINPUT_GAMEPAD_BACK, NewController->State);
							//bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							//bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						}
						else{
								//NOTE: Controller not available.
						}
					}
					
					
					https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708932(v=vs.85)
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
					
					GameUpdateRenderer(NewInput, &Buffer,&SoundBuffer);
					
					if(SoundIsValid){
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}
					
					win32_window_dimension Dimension = Win32GetWindowDimension(TheWindow);
					Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
					ReleaseDC(TheWindow, DeviceContext); //Was this moved or deleted?? Or not needed because we only create 1 window??
					
					
					//This would test code before compiler optizes. In build script add -O2 flag to get compiler optimized code.
					int64 EndCycleCount = __rdtsc();
					
					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);
					
					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					real32 MSPerFrame = (((1000.0f*(real32)CounterElapsed) / (real32)PerfCounterFrequency));
					real32 FPS = (real32)PerfCounterFrequency / (real32)CounterElapsed;
					real32 MegaCyclesPerFrame = ((real32)CyclesElapsed/(1000.0f*1000.0f));
#if 0
					char Buffer[256];
					//C Stanrd Library (stdio.h sprintf) & Windows
					sprintf(Buffer, "Milliseconds/frame %f ms / %f FPS %f MegaCycles/frame\n", MSPerFrame, FPS, MegaCyclesPerFrame);
					OutputDebugStringA(Buffer);//My value differ drastically from Casey's. Casey was using OBS at the same time. My processor is 8 years newer but I was getting .00008 FPS when Casey was getting 8FPS?????? Did my machine use the powerful NVidia GPU?
#endif
					
					LastCycleCount = EndCycleCount;
					LastCounter = EndCounter;
					
					
					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
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