///////////////////////////
// Handmade Hero Game    //
// Author: Adam Johnston //
// Created: 3/12/2020    //
///////////////////////////


#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <cmath>

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

typedef int32 bool32;

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

// TODO(adam): This is a global for now
global_variable bool                   GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER    GlobalSecondaryBuffer;

// NOTE(adam): Support for XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(adam): Support for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

/*
 * Try to load XInput library
 */
internal void Win32LoadXInput()
{
    // TODO(adam): Check Windows 8 compatability
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) { XInputGetState = XInputGetStateStub; }

        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
    }
}

/*
 * Load and initialize DirectSound
 */
internal void Win32InitDSound(HWND  Window,
                              int32 SamplesPerSecond,
                              int32 BufferSize)
{
    // NOTE(adam): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // NOTE(adam): Get a DirectSound object! - cooperative
        direct_sound_create *DirectSoundCreate =
            (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO(adam): Double check DirectSound8 or 7 ?? works on XP
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // NOTE(adam): "Create" a primary buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // TODO(adam): DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER  PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error))
                    {
                        // NOTE(adam): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(adam): Diagnostic
                    }
                }
            }
            else
            {
                // TODO(adam): Diagnostic
            }

            // NOTE(adam): "Create" a secondary buffer
            // TODO(adam): DSBCAPS_GETCURRENTPOSITION2 ?
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            // TODO(adam): DSBCAPS_GLOBALFOCUS?
            GlobalSecondaryBuffer;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary buffer created successfully\n");
            }

            // NOTE(adam): Start it playing!
        }
        else
        {
            // TODO(adam): Diagnostic
        }

    }
    else
    {
        // TODO(adam): Diagnostic
    }
}

/*
 * Paint gradient
 */
internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer,
                                  int                     XOffset,
                                  int                     YOffset,
                                  int                     RedShift)
{
    // TODO(adam): See what the optimizer does when we pass Buffer by value
    // NOTE(adam): See Chandler Carruth lectures for optimization (contributor to LLVM)
    uint8 *Row = (uint8 *)Buffer->Memory;
    for(int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            /*
            Little endian architecture so bytes are reversed
            Pixel:
                Register:   xx RR GG BB
                Memory:     BB GG RR XX
            */
            // uint8 Red = (X + YOffset) - (Y + XOffset);
            uint8 Blue = sin(X) * XOffset;
            uint8 Green = cos(Y) * YOffset;
            uint8 Red = Blue + Green + RedShift;

            *Pixel++ = (uint32)((Red << 16) | (Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

/*
 * Get window dimensions
 */
internal win32_window_dimensions Win32GetWindowDimensions(HWND Window)
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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;

    // TODO(adam): Probably clear this to black

}

/*
 * Paint to the bitmap
 */
internal void Win32DisplayBufferInWindow(HDC                     DeviceContext,
                                         win32_offscreen_buffer *Buffer,
                                         int                     WindowWidth,
                                         int                     WindowHeight)
{
    // TODO(adam): Aspect ratio
    // TODO(adam): Play with stretch modes
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

/*
 * Message callbacks
 */
internal LRESULT CALLBACK Win32MainWindowCallback(HWND   Window,
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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown  = ((LParam & (1 << 31)) == 0);
            if (WasDown != IsDown)
            {
                if      (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_DOWN)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if (IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if (WasDown)
                    {
                        OutputDebugStringA("WasDown ");
                    }
                    OutputDebugStringA("\n");
                }
                else if (VKCode == VK_SPACE)
                {
                }
            }
            bool32 AltKeyWasDown = (LParam & (1 << 29));
            if ((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
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
                &GlobalBackBuffer,
                Dimension.Width,
                Dimension.Height);
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
     * Define Xinput functions
     */
    Win32LoadXInput();

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

            int    SamplesPerSecond     = 48000;
            int    XOffset              = 0;
            int    YOffset              = 0;
            int    RedShift             = 100;
            int    ToneHz               = 256;
            int    ToneVolume           = 200;
            uint32 RunningSampleIndex   = 0;
            int    SquareWaveCounter    = 0;
            int    SquareWavePeriod     = SamplesPerSecond / ToneHz;
            int    HalfSquareWavePeriod = SquareWavePeriod / 2;
            int    BytesPerSample       = sizeof(int16) * 2;
            int    SecondaryBufferSize  = SamplesPerSecond * BytesPerSample;

            Win32InitDSound(Window, SamplesPerSecond, SamplesPerSecond * BytesPerSample);
            bool32 SoundIsPlaying = false;

            GlobalRunning = true;
            bool ControllerFound = false;
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

                        ControllerFound = true;

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

                        XOffset -= StickX >> 12;
                        YOffset += StickY >> 12;

                        if (AButton) { RedShift += 5; }
                        if (BButton) { RedShift -= 5; }

                    }
                    else
                    {
                        // NOTE(adam): This controller is not available
                        if (ControllerIndex == 0 && ControllerFound)
                        {
                            MessageBox(Window, "Fuck, you lost your controller!!!!!", "FUCK!", MB_OK);
                            ControllerFound = false;
                        }
                    }
                }

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset, RedShift);

                // NOTE(adam): DirectSound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock = (RunningSampleIndex * BytesPerSample) % SecondaryBufferSize;
                    DWORD BytesToWrite;
                    if (ByteToLock == PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize;
                    }
                    else if (ByteToLock > PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize - ByteToLock;
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    VOID *Region1;
                    DWORD Region1Size;
                    VOID *Region2;
                    DWORD Region2Size;
                    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                                              &Region1, &Region1Size,
                                                              &Region2, &Region2Size,
                                                              0)))
                    {
                        // TODO(adam): assert Region1Size/Region2Size is valid
                        DWORD Region1SampleCount = Region1Size / BytesPerSample;
                        DWORD Region2SampleCount = Region2Size / BytesPerSample;

                        // TODO(adam): Collapse these loops
                        int16 *SampleOut = (int16 *)Region1;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region1SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue; // L
                            *SampleOut++ = SampleValue; // R
                        }

                        SampleOut = (int16 *)Region2;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region2SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue; // L
                            *SampleOut++ = SampleValue; // R
                        }
                        GlobalSecondaryBuffer->Unlock(Region1, Region1Size,
                                                      Region2, Region2Size);
                    }
                }

                if (!SoundIsPlaying)
                {
                    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                    SoundIsPlaying = true;
                }

                win32_window_dimensions Dimension = Win32GetWindowDimensions(Window);
                Win32DisplayBufferInWindow(
                    DeviceContext,
                    &GlobalBackBuffer,
                    Dimension.Width,
                    Dimension.Height);
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
