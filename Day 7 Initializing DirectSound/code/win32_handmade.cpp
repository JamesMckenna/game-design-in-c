#include <windows.h>
#include <stdint.h> //int datatypes redifined from original C spec
#include <xinput.h>
#include <dsound.h>

// #define explanation https://en.cppreference.com/w/c/preprocessor/replace
#define internal static  //scopes this static var to a file
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
typedef int32 bool32; //In C 0 is false, 1 is true for bool datatype. This custom datatype says all I care about is 0 or some numerical value; don't care if value is 1 or 230000. if 0 then false, if not 0, then true.

struct win32_offscreen_buffer {
	BITMAPINFO Info;
	void *Memory; //void * means give me a pointer, but not sure what datatype or format it's going to be. Later we can/will cast when needed
	int Width;
	int Height;
	int Pitch;
};

struct win32_window_dimension {
	int Width;
	int Height;
};

/* GAMEPAD DLL MISSING GUARD STARTS HERE */
//Here copied and tweak these function directly from the dll source file (xinput.h) becuase we are not too sure if available on players OS version
//Now the compiler can link to these definitions because we typedef'ed them. also renamed them so theres no conflict with dll if avaialable, then #define to map to our fake pointer.
//This will prevent the app from crahing on start; because now there is something to link to. If they ever get called, the Stub will handle it and we won't have an Access Violation Linking error.
//SO if there is no gamepad dll, the app won't crash and user can still use keyboard controllers.
//DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);
//DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
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

//Now if dll is available, we load it the same way windows would
//https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya
HMODULE XInputLibrary = LoadLibrary("XInput1_4.dll");//we can get the library name and versions from <xinput.h>. To check if windows machine has, type C:\Windows>dir /s xinput1_4.dll into cmd
internal void Win32LoadXInput(void){
	if(XInputLibrary){
			XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");//https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress
			XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");// GetProcAddress doesn't know what the type of the function it's loading is, so we need to cast it. 
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


global_variable BOOL Running; //the static keyword will automatically initialize this var to zero
global_variable win32_offscreen_buffer GlobalBackBuffer;


internal win32_window_dimension Win32GetWindowDimension(HWND Window){
	
	win32_window_dimension Result;
	
	RECT ClientRect;
	//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
	//get the drawable area; not including any border, padding or margin.
	GetClientRect(Window, &ClientRect);
	Result.Height = ClientRect.bottom - ClientRect.top;
	Result.Width = ClientRect.right - ClientRect.left;
	
	return Result;
}

internal void RenderWierdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset){

	uint8 *Row = (uint8 *)Buffer->Memory;
	for(int Y = 0; Y < Buffer->Height; ++Y){
		uint32 *Pixel = (uint32 *)Row;
		//Recall that Little Endian Archtecture reads from largest back. So in memory, it's 0x00BBGGRR
		for(int X = 0; X < Buffer->Width; ++X){
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue); //the *Pixel++  means deref Pixel address, use Pixel value, THEN increment Pixel value.
		}
		Row += Buffer->Pitch;//Here we make sure that Row is reset including any byte padding we have that is not being caught by ++Pixel
	}
}

//DIB = Device Independant Bitmap: a way to do graphics that windows can understand 
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height){
	
	//TODO: bullet proof this. Free up old buffer before overwritting. don't free first, then free first if that fails???
	//TODO: Free our DIBSection
	
	if(Buffer->Memory){
		//https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	int BytesPerPixel = 4;
	
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfo & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
	//Something to note: No Color Table Used!!!! We are going to feed color values as RGB; NOT a Color Palet look up table
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;//Make this a negative value cause we want to start drawing from top left down. SEE: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;//8 bits for Red, 8 bits for Blue, 8 bits for Green  = 24 bits. BUT we want to make sure the memory is aligned, so reserve 32 bits. The last 8 bits is a (memory) padding to ensure we align to 4 bit, 8 bit, 16 bit and 32 bit sections of ram. 4 byte boundries 
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;//Because we aligned to 4 byte boundries, we know that we are going to need 4 bytes
	//https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	
	//This has to do with Memory byte Stride and Pitch. Where Pitch is byte to byte and stride would be the whole 32 byte allocation
	Buffer->Pitch = Width*BytesPerPixel;
}

//The Win32 is our prefix to tell us this is code for windows platform; the UpdateWindow is a windows platform keyword
internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer *Buffer){
											
	//TODO: Aspect Ratioo correction needed.
	
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt
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
			LPDIRECTSOUNDBUFFER SecondaryBuffer;
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0))){
				//Start playing sound
				OutputDebugStringA("Secondary Buffer created\n");
			}
			
		}
		else {
			//TODO handle no direct sound, Diagnostic
		}
	}
}


//Wndproc is the MainWindowCallback WinMain Callback. This handles messages (message que GetMessageA) sent to the window 
//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc 
//@Param MessageToWindow https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues#system-defined-messages NOTE: will not use most as we just want the Window's window and we will write most else. If not coding everything else, we might want to better understand all the messages args that can be passed to this param 
LRESULT Wndproc(HWND WindowToHandle, UINT MessageToWindow, WPARAM WParam, LPARAM LParam){
	LRESULT Result = 0;
	//https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues#system-defined-messages
	//https://learn.microsoft.com/en-us/windows/win32/winmsg/window-notifications
	switch(MessageToWindow){
		
		case WM_SIZE:
		{
			//win32_window_dimension Dimension = Win32GetWindowDimension(WindowToHandle);
			//Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);	
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
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage
			//PostQuitMessage(0); This is one way to quit. But using bool flag and handle all cleanup that way. 
			
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
			//https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
			int32 VKCode = WParam;
			BOOL WasDown = ((LParam & (1 << 30)) != 0); //C will return zero or one instead of 30 is key was down
			BOOL IsDown = ((LParam & (1 << 31)) == 0); //C will return zero or one instead of 30 is key is down
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
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-paintstruct
			PAINTSTRUCT Paint; 
			
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-beginpaint
			HDC DeviceContext = BeginPaint(WindowToHandle, &Paint);
			
			win32_window_dimension Dimension = Win32GetWindowDimension(WindowToHandle);	
			Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);
		
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-endpaint
			EndPaint(WindowToHandle, &Paint);
		}break;
		default:
		{
			OutputDebugStringA("DEFAULT CASE\n");
			//Window's window message default handler https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-defwindowproca
			Result = DefWindowProc(WindowToHandle, MessageToWindow, WParam, LParam);
		} break;
	}//NOTE: Casey suggested using block scoping for each case statement - makes sense to me
	
	return Result;
}

//https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-winmain
int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
	{ 
		Win32LoadXInput();
		WNDCLASS WindowClass = {}; //https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassa
		
		Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
		
		WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW; //THIS say redraw whole window https://learn.microsoft.com/en-us/windows/win32/winmsg/about-window-classes#class-styles	https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
		WindowClass.lpfnWndProc = Wndproc; //Handles all message coming from Windows
		WindowClass.hInstance = hInstance;
		//WindowClass.hIcon;
		WindowClass.lpszClassName = "HandmadeHeroWindowClass";

		//We have to register the window that is Wndproc
		//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa
		if(RegisterClassA(&WindowClass))
		{
			//Alls Good! //https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa
			//@Param[4] dwStyle https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
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
				//So if the window is created with CreateWindowExA, we need to start a message loop.
				//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagea
				Running = true;
				int xOffset = 0;
				int yOffset = 0;
				
				//Now that we have a window, we can create and load our sound (DirectSound)
				Win32InitDSound(TheWindow, 48000, 48000*sizeof(int16)*2);
				
				while(Running){
					//https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msg
					MSG Message;
					while(PeekMessageA(&Message,0,0,0, PM_REMOVE)){
						
						if(Message.message == WM_QUIT){
							Running = false;
						}
						
						//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage
						TranslateMessage(&Message);
						//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessage
						DispatchMessageA(&Message);
					}
					
					//https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput#using-xinput
					for(DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex){
						XINPUT_STATE ControllerState;
						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS){
							//NOTE: This controller is plugged in.
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