///////////////////////////
// Handmade Hero Game    //
// Author: Adam Johnston //
// Created: 3/12/2020    //
///////////////////////////
#include <windows.h>
#include <stdint.h>

#define local_persist static
#define global_variable static
#define internal static

typedef  uint8_t  uint8;
typedef  uint16_t uint16;
typedef  uint32_t uint32;
typedef  uint64_t uint64;

typedef  int8_t  int8;
typedef  int16_t int16;
typedef  int32_t int32;
typedef  int64_t int64;

// TODO(adam): This is a global for now
global_variable bool Running;
global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;

/*
 * Resize bitmap
 */
internal void Win32ResizeDIBSection(int Width,
                                    int Height)
{
    // TODO(adam): Bulletproof this
    // Mayby don't free first, free after if that fails

    if (BitmapMemory)
    {
        // NOTE(adam): Can use VirtualProtect and remove read-write
        // access to track down use-after-free bugs
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = Width;
    BitmapHeight = Height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // (-) makes it top-down bitmap
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    // NOTE(adam): Thanks to Chris Hecker of Spy Party fame
    // for pointing out we don't need to use DC if we are
    // using StretchDIBits instead of BitBlt!
    int BytesPerPixel = 4; // Only need 3, but 4 will keep us aligned
    int BitmapMemorySize = BytesPerPixel * BitmapWidth * BitmapHeight;
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    int Pitch = Width * BytesPerPixel;
    uint8 *Row = (uint8 *)BitmapMemory;
    for(int Y = 0; Y < BitmapHeight; ++Y)
    {
        uint8 *Pixel = (uint8 *)Row;
        for (int X = 0; X < BitmapWidth; ++X)
        {
            /*
              Little endian architecture so bytes are reversed
              Actual memory:   xx RR GG BB
              Pixel in memory: BB GG RR XX
            */
            // Blue
            *Pixel = (uint8)X;
            ++Pixel;

            // Green
            *Pixel = (uint8)Y;
            ++Pixel;

            // Red
            *Pixel = 0;
            ++Pixel;

            // Pad
            *Pixel = 0;
            ++Pixel;
        }

        Row += Pitch;
    }
}

/*
 * Paint to the bitmap
 */
internal void Win32UpdateWindow(HDC DeviceContext,
                                RECT *WindowRect,
                                int X,
                                int Y,
                                int Width,
                                int Height)
{
    int WindowWidth = WindowRect->right - WindowRect->left;
    int WindowHeight = WindowRect->bottom - WindowRect->top;
    StretchDIBits(
        DeviceContext,
        /* Dirty Rectangle
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, BitmapWidth, BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);
}

/*
 * Message callbacks
 */
LRESULT CALLBACK Win32MainWindowCallback(HWND   Window,
                                         UINT   Message,
                                         WPARAM WParam,
                                         LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int Height = ClientRect.bottom - ClientRect.top;
            int Width = ClientRect.right - ClientRect.left;
            Win32ResizeDIBSection(Width, Height);
        } break;

        case WM_DESTROY:
        {
            // TODO(adam): Handle this with error - recreate window?
            Running = false;
        } break;

        case WM_CLOSE:
        {
            // TODO(adam): Handle this with message to user?
            Running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);

            Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            // OutputDebugStreamA("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

/*
 * Main function called by Windows
 */
int WINAPI wWinMain(HINSTANCE Instance,
                    HINSTANCE PrevInstance,
                    PWSTR     CommandLine,
                    int       ShowCode)
{
    /*
     * Define window class
     */
    WNDCLASSA WindowClass = {};
    // TODO(adam): Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    /*
     * Register our window class
     */
    if (RegisterClass(&WindowClass))
    {
        HWND WindowHandle =
            CreateWindowExA(
                0,
                WindowClass.lpszClassName,
                "Handmade Hero",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);
        /*
         * If successful, loop through messages
         */
        if (WindowHandle)
        {
            Running = true;
            while (Running)
            {
                MSG Message;
                BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);
                if (MessageResult > 0)
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                else
                {
                    break;
                }
            }
        }
        // Failed to create window
        else
        {
            // TODO(adam): Logging
        }
    }
    // Failed to register window class
    else
    {
        // TODO(adam): Logging
    }

    return 0;
}
