#include <windows.h>
#include <stdint.h> 
#include <xinput.h>
#include <dsound.h>

#define internal static  
#define local_persist static 
#define global_variable static 

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32; 

struct win32_offscreen_buffer {
	BITMAPINFO Info;
	void *Memory; 
	int Width;
	int Height;
	int Pitch;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

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

HMODULE XInputLibrary = LoadLibrary("XInput1_4.dll");
internal void Win32LoadXInput(void){
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


global_variable BOOL Running;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal win32_window_dimension Win32GetWindowDimension(HWND Window){
	
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;
	
	return Result;
}

internal void RenderWierdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset){

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
			BOOL WasDown = ((LParam & (1 << 30)) != 0);
			BOOL IsDown = ((LParam & (1 << 31)) == 0);
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
			Result = DefWindowProc(WindowToHandle, MessageToWindow, WParam, LParam);
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
				Running = true;
				int xOffset = 0;
				int yOffset = 0;
				
				int SamplesPerSecond = 48000;
				int ToneHz = 256; //Middle C is 261.625565Hz, so 256 will be flat
				int16 ToneVolume = 2000;
				uint32 RunningSampleIndex = 0;
				//int SquareWaveCounter = 0;
				int SquareWavePeriod = SamplesPerSecond/ToneHz;
				int HalfSquareWavePeriod = SquareWavePeriod/2;
				int BytesPerSample = sizeof(int16)*2;
				int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;
				
				Win32InitDSound(TheWindow, SamplesPerSecond, SecondaryBufferSize);
				bool32 SoundIsPlaying = false;
				
				
				
				while(Running){
					MSG Message;
					while(PeekMessageA(&Message,0,0,0, PM_REMOVE)){
						
						if(Message.message == WM_QUIT){
							Running = false;
						}
						
						TranslateMessage(&Message);
						DispatchMessageA(&Message);
					}
					
					for(DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex){
						XINPUT_STATE ControllerState;
						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS){
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
							BOOL Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							BOOL Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							BOOL Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							BOOL Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
							BOOL Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							BOOL Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
							BOOL LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
							BOOL RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
							BOOL ABtn = (Pad->wButtons & XINPUT_GAMEPAD_A);
							BOOL BBtn = (Pad->wButtons & XINPUT_GAMEPAD_B);
							BOOL XBtn = (Pad->wButtons & XINPUT_GAMEPAD_X);
							BOOL YBtn = (Pad->wButtons & XINPUT_GAMEPAD_Y);
							
							int16 StickX = Pad->sThumbLX;
							int16 StickY = Pad->sThumbLY;
							
							//to test connection
							if(ABtn){
								yOffset += 2;
								++xOffset;
								XINPUT_VIBRATION Vibration;
								Vibration.wLeftMotorSpeed = 60000;
								Vibration.wRightMotorSpeed = 60000;
								XInputSetState(0, &Vibration);
							}
							xOffset += StickX >> 12;
							yOffset += StickY >> 12;
						}
						else{
								//NOTE: Controller not available.
						}
					}
									
					RenderWierdGradient(&GlobalBackBuffer, xOffset, yOffset);
					
					https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708932(v=vs.85)
					DWORD PlayCursor;
					DWORD WriteCursor;
					if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))){
						DWORD ByteToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize; //Mod by BufferSize so if it loops, start writing back at start of bufffer
						DWORD BytesToWrite;
						if(ByteToLock == PlayCursor){
							BytesToWrite = SecondaryBufferSize;
						}	
						else if(ByteToLock > PlayCursor){
							BytesToWrite = (SecondaryBufferSize - ByteToLock);
							BytesToWrite += PlayCursor;
						}
						else{
							BytesToWrite = PlayCursor - ByteToLock;
						}
						
						// Need to write left and right channels together. 
						// |int16	int16|	|int16	int16|	|int16	int16|
						// |Left	Right|	|Left	Right|	|Left	Right|
						VOID *Region1;
						DWORD Region1Size;
						VOID *Region2;
						DWORD Region2Size;
						
						if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))){
							
							DWORD Region1SampleCount = Region1Size/BytesPerSample;
							int16 *SampleOut = (int16 *)Region1;
							for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex){
								int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
								*SampleOut++ = SampleValue;
								*SampleOut++ = SampleValue;
							}
							
							DWORD Region2SampleCount = Region2Size/BytesPerSample;
							SampleOut = (int16 *)Region2;
							for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex){
								int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) %2) ? ToneVolume : -ToneVolume;
								*SampleOut++ = SampleValue;
								*SampleOut++ = SampleValue;
							}
							
							GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
						}
					}
					
					if(!SoundIsPlaying){
						GlobalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING);
						SoundIsPlaying = true;
					}
					
					HDC DeviceContext = GetDC(TheWindow);
					win32_window_dimension Dimension = Win32GetWindowDimension(TheWindow);
					Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
					ReleaseDC(TheWindow, DeviceContext);	
				
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