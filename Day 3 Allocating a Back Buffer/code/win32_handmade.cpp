#include <windows.h>
// #define explanation https://en.cppreference.com/w/c/preprocessor/replace
#define internal static  //scopes this static var to a file
#define local_persist static 
#define global_variable static 

//This is a global for now.
global_variable BOOL Running; //the static keyword will automatically initialize this var to zero
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory; //void * means give me a pointer, but not sure what datatype or format it's going to be. Later we can/will cast when needed
global_variable HBITMAP BitmapHandle;
global_variable HDC BitmapDeviceContext;

//DIB = Device Independant Bitmap: a way to do graphics that windows can understand 
internal void ResizeDIBSection(int Width, int Height){
	
	//TODO: bullet proof this. Free up old buffer before overwritting. don't free first, then free first if that fails???
	//TODO: Free our DIBSection
	if(BitmapHandle){
		DeleteObject(BitmapHandle);
	}
	
	if(!BitmapDeviceContext){
		//Should this be deleted and recreated if for example, a monitor gets unplugged and new one or different plugged in
		//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createcompatibledc
		BitmapDeviceContext = CreateCompatibleDC(0);
	}
	
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfo & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader
	//Something to note: No Color Table Used!!!! We are going to feed color values as RGB; NOT a Color Palet look up table
	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = Width;
	BitmapInfo.bmiHeader.biHeight = Height;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
	/* 
	Because C automatically initializes to Zero, these member values are already zero
	BitmapInfo.biSizeImage = 0;
	BitmapInfo.biXPelsPerMeter = 0;
	BitmapInfo.biYPelsPerMeter = 0;
	BitmapInfo.biClrUsed = 0;
	BitmapInfo.biClrImport = 0;
	*/
	
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection
	BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitmapInfo, DIB_RGB_COLORS, &BitmapMemory, 0, 0); //TODO, NOTE: based on ssylvan's suggestion, we might be able to allocate the memoey ourselves rather the using a windows' api
}

//The Win32 is our prefix to tell us this is code for windows platform; the UpdateWindow is a windows platform keyword
internal void Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height){
	//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits & https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt
	StretchDIBits(DeviceContext, 
			X, Y, Width, Height,
			X, Y, Width, Height,
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
			OutputDebugStringA("WM_SIZE\n");
			RECT ClientRect;
			//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
			//get the drawable area; not including any border, padding or margin.
			GetClientRect(WindowToHandle, &ClientRect);
			int Height = ClientRect.bottom - ClientRect.top;
			int Width = ClientRect.right - ClientRect.left;
			ResizeDIBSection(Width, Height);	
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
			
			Win32UpdateWindow(DeviceContext, X, Y, Width, Height);
		
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
			HWND WindowHandle = CreateWindowExA(
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
				
			if(WindowHandle){
				//So if the window is created with CreateWindowExA, we need to start a message loop.
				//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessage
				//@Param[1] arg=0 or GRAB all messages to (any?) window
				Running = true;
				while(Running){
					MSG Message;
					BOOL MessageResult = GetMessageA(&Message,0,0,0);
					if(MessageResult > 0){
						//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage
						TranslateMessage(&Message);
						//https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessage
						DispatchMessageA(&Message);
					}
					else{
						break;
					}
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
    