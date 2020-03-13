///////////////////////////
// Handmade Hero Game    //
// Author: Adam Johnston //
// Created: 3/12/2020    //
///////////////////////////


#include <windows.h>
#include <stdint.h>
#include <xinput.h>

#define local_persist   static
#define global_variable static
#define internal        static

typedef  uint8_t  uint8;
typedef  uint16_t uint16;
typedef  uint32_t uint32;
typedef  uint64_t uint64;

typedef  int8_t  int8;
typedef  int16_t int16;
typedef  int32_t int32;
typedef  int64_t int64;

struct win32_offscreen_buffer
{
    BITMAPINFO  Info;
    void       *Memory;
    int         Width;
    int         Height;
    int         Pitch;
};

struct win32_window_dimensions
{
    int Width;
    int Height;
};

// NOTE(adam): Support for XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

// NOTE(adam): Support for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

// TODO(adam): This is a global for now
global_variable bool                   GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

/*
 * Paint gradient
 */
internal void RenderWeirdGradient(win32_offscreen_buffer Buffer,
                                  int                    XOffset,
                                  int                    YOffset)
{
    // TODO(adam): See what the optimizer does when we pass Buffer by value
    // NOTE(adam): See Chandler Carruth lectures for optimization (contributor to LLVM)
    uint8 *Row = (uint8 *)Buffer.Memory;
    for(int Y = 0; Y < Buffer.Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer.Width; ++X)
        {
            /*
            Little endian architecture so bytes are reversed
            Pixel:
                Register:   xx RR GG BB
                Memory:     BB GG RR XX
            */
            // uint8 Red = (X + YOffset) - (Y + XOffset);
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);
            uint8 Red = Blue + Green;

            *Pixel++ = (uint32)((Red << 16) | (Green << 8) | Blue);
        }

        Row += Buffer.Pitch;
    }
}

/*
 * Get window dimensions
 */
win32_window_dimensions Win32GetWindowDimensions(HWND Window)
{
    win32_window_dimensions Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Height = ClientRect.bottom - ClientRect.top;
    Result.Width = ClientRect.right - ClientRect.left;

    return Result;
}

/*
 * Resize bitmap
 */
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer,
                                    int                     Width,
                                    int                     Height)
{
    // TODO(adam): Bulletproof this
    // Mayby don't free first, free after if that fails

    if (Buffer->Memory)
    {
        // NOTE(adam): Can use VirtualProtect and remove read-write
        // access to track down use-after-free bugs
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    int BytesPerPixel = 4; // Only need 3, but 4 will keep us aligned

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // (-) makes it top-down bitmap
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE(adam): Thanks to Chris Hecker of Spy Party fame
    // for pointing out we don't need to use DC if we are
    // using StretchDIBits instead of BitBlt!
    int BitmapMemorySize = BytesPerPixel * Buffer->Width * Buffer->Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;

    // TODO(adam): Probably clear this to black

}

/*
 * Paint to the bitmap
 */
internal void Win32DisplayBufferInWindow(HDC                    DeviceContext,
                                         int                    WindowWidth,
                                         int                    WindowHeight,
                                         win32_offscreen_buffer Buffer)
{
    // TODO(adam): Aspect ratio
    // TODO(adam): Play with stretch modes
    StretchDIBits(
        DeviceContext,
        /* Dirty Rectangle
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer.Width, Buffer.Height,
        Buffer.Memory,
        &Buffer.Info,
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
        case WM_DESTROY:
        {
            // TODO(adam): Handle this with error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_CLOSE:
        {
            // TODO(adam): Handle this with message to user?
            GlobalRunning = false;
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

            win32_window_dimensions Dimension = Win32GetWindowDimensions(Window);
            Win32DisplayBufferInWindow(
                DeviceContext,
                Dimension.Width,
                Dimension.Height,
                GlobalBackBuffer);
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

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    /*
     * Register our window class
     */
    if (RegisterClass(&WindowClass))
    {
        HWND Window =
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
        if (Window)
        {
            // NOTE(adam): Since we specified CS_OWNDC, we can just
            // get one DC and use it forever because we are not
            // sharing it
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            GlobalRunning = true;
            while (GlobalRunning)
            {
                MSG Message;
                // vvv Blocks
                // BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                // TODO(adam): Should we poll this more frequently?
                for (DWORD ControllerIndex = 0;
                     ControllerIndex < XUSER_MAX_COUNT;
                    ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE(adam): This controller is plugged in
                        // TODO(adam): See if ControllerState.dwPacketNumber increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool  Up            = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool  Down          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool  Left          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool  Right         = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool  Start         = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool  Back          = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool  LeftShoulder  = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool  RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool  AButton       = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool  BButton       = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool  XButton       = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool  YButton       = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        int16 StickX        = Pad->sThumbLX;
                        int16 StickY        = Pad->sThumbLY;
                    }
                    else
                    {
                        // NOTE(adam): This controller is not available
                    }
                }

                RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);

                win32_window_dimensions Dimension = Win32GetWindowDimensions(Window);
                Win32DisplayBufferInWindow(
                    DeviceContext,
                    Dimension.Width,
                    Dimension.Height,
                    GlobalBackBuffer);

                ++XOffset;
                ++YOffset;
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
