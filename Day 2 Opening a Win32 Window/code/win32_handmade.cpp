#include <windows.h>

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
		} break;
		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
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
			
			//https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-patblt NEEDS: Gdi32.lib
			PatBlt(DeviceContext, X, Y, Width, Height, WHITENESS);
			
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
				
				for(;;){
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
    