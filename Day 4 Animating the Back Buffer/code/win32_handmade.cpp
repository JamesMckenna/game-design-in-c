#include <windows.h>
#include <stdint.h> //int datatypes redifined from original C spec

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

//This is a global for now.
global_variable BOOL Running; //the static keyword will automatically initialize this var to zero
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory; //void * means give me a pointer, but not sure what datatype or format it's going to be. Later we can/will cast when needed
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;


internal void RenderWierdGradient(int xOffset, int yOffset){
	
	int Width = BitmapWidth;
	int Height = BitmapHeight;
	
	//This has to do with Memory byte Stride and Pitch. Where Pitch is byte to byte and stride would be the whole 32 byte allocation
	int Pitch = Width*BytesPerPixel;
	uint8 *Row = (uint8 *)BitmapMemory;
	for(int Y = 0; Y < BitmapHeight; ++Y){
		uint32 *Pixel = (uint32 *)Row;
		//Recall that Little Endian Archtecture reads from largest back. So in memory, it's 0x00BBGGRR
		for(int X = 0; X < BitmapWidth; ++X){
			uint8 Blue = (X + xOffset);
			uint8 Green = (Y + yOffset);
			*Pixel++ = ((Green << 8) | Blue); //the *Pixel++  means deref Pixel address, use Pixel value, THEN increment Pixel value.
		}
		Row += Pitch;//Here we make sure that Row is reset including any byte padding we have that is not being caught by ++Pixel
	}
}

//DIB = Device Independant Bitmap: a way to do graphics that windows can understand 
internal void Win32ResizeDIBSection(int Width, int Height){
	
	//TODO: bullet proof this. Free up old buffer before overwritting. don't free first, then free first if that fails???
	//TODO: Free our DIBSection
	
	if(BitmapMemory){
		//https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}
	
	BitmapWidth = Width;
	BitmapHeight = Height;
	
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfo & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
	//Something to note: No Color Table Used!!!! We are going to feed color values as RGB; NOT a Color Palet look up table
	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;//Make this a negative value cause we want to start drawing from top left down. SEE: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;//8 bits for Red, 8 bits for Blue, 8 bits for Green  = 24 bits. BUT we want to make sure the memory is aligned, so reserve 32 bits. The last 8 bits is a (memory) padding to ensure we align to 4 bit, 8 bit, 16 bit and 32 bit sections of ram. 4 byte boundries 
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (BitmapWidth*BitmapHeight)*BytesPerPixel;//Because we aligned to 4 byte boundries, we know that we are going to need 4 bytes
	//https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

//The Win32 is our prefix to tell us this is code for windows platform; the UpdateWindow is a windows platform keyword
internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height){
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt
	StretchDIBits(DeviceContext, 
			//X, Y, Width, Height,
			//X, Y, Width, Height,
			0, 0, BitmapWidth, BitmapHeight,
			0, 0, WindowWidth, WindowHeight,
			BitmapMemory,
			&BitmapInfo,
			DIB_RGB_COLORS, SRCCOPY);
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
			//OutputDebugStringA("WM_SIZE\n");
			RECT ClientRect;
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
			//get the drawable area; not including any border, padding or margin.
			GetClientRect(WindowToHandle, &ClientRect);
			int Height = ClientRect.bottom - ClientRect.top;
			int Width = ClientRect.right - ClientRect.left;
			Win32ResizeDIBSection(Width, Height);	
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
		case WM_PAINT:
		{
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-paintstruct
			PAINTSTRUCT Paint; 
			
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-beginpaint
			HDC DeviceContext = BeginPaint(WindowToHandle, &Paint);
			
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			
			//RECT ClientRect;
			//GetClientRect(WindowToHandle, &ClientRect);
			//Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
		
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
		WNDCLASS WindowClass = {}; //https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassa
		WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW; //THIS class attribute does not need to be initialized https://learn.microsoft.com/en-us/windows/win32/winmsg/about-window-classes#class-styles	https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
		WindowClass.lpfnWndProc = Wndproc; //Handles all message coming from Windows
		WindowClass.hInstance = hInstance;
		//WindowClass.hIcon = ;
		//WindowClass.hbrBackground;
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
					RenderWierdGradient(xOffset, yOffset);
					
					HDC DeviceContext = GetDC(TheWindow);	
					RECT ClientRect;
					GetClientRect(TheWindow, &ClientRect);
					int WindowWidth = ClientRect.right - ClientRect.left;
					int WindowHeight = ClientRect.bottom - ClientRect.top;
					Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
					ReleaseDC(TheWindow, DeviceContext);
					
					
					++xOffset;
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