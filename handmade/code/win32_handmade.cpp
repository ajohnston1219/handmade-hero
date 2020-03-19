///////////////////////////
// Handmade Hero Game    //
// Author: Adam Johnston //
// Created: 3/12/2020    //
///////////////////////////

/*
   TODO(adam): THIS IS NOT A FINAL PLATFORM LAYER!!!

   - Saved game locations
   - Getting a handle to our own executable file
   - Asset loading path
   - Threading (launch a thread)
   - Raw input (support multiple keyboards)
   - Sleep/timeBeginPeriod
   - ClipCursor() (for multimonitor support)
   - WM_SETCURSOR (control cursor visibility)
   - WM_ACTIVATEAPP (for when we are not active application)
   - Blit speed improvements (BitBlit)
   - Hardware acceleration (OpenGL or Direct3D or BOTH??)
   - GetKeyboardLayout (for French keyboards, international WASD support)

 */

#include <stdint.h>

#define Pi32 3.1415926535897932384626433

typedef  uint8_t  uint8;
typedef  uint16_t uint16;
typedef  uint32_t uint32;
typedef  uint64_t uint64;

typedef  int8_t  int8;
typedef  int16_t int16;
typedef  int32_t int32;
typedef  int64_t int64;

typedef float  real32;
typedef double real64;

typedef int32 bool32;

#include <windows.h>
#include "handmade.cpp"

#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO(adam): This is a global for now
global_variable bool32                 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER    GlobalSecondaryBuffer;
global_variable int64                  GlobalPerfCountFrequency;


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

inline uint32 SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return Result;
}

/*
 * File I/O
 */
internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            // TODO(adam): Defines for maximum values
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    (FileSize32 == BytesRead))
                {
                    // NOTE(adam): File read successfully
                    Result.ContentSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
            // Error allocating
            else
            {
                // TODO(adam): Logging
            }
        }
        // Error getting file size
        else
        {
            // TODO(adam): Logging
        }

        CloseHandle(FileHandle);
    }
    // Invalid file handle
    else
    {
        // TODO(adam): Logging
    }

    return Result;
}

internal void DEBUGPlatformFreeFileMemory(void *Memory)
{
    if (Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

internal bool32 DEBUGPlatformWriteEntireFile(char   *Filename,
                                             uint32  MemorySize,
                                             void   *Memory)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(adam): File read successful
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            // TODO(adam): Logging
        }

        CloseHandle(FileHandle);
    }
    // Invalid file handle
    else
    {
        // TODO(adam): Logging
    }

    return Result;
}

/*
 * Try to load XInput library
 */
internal void Win32LoadXInput()
{
    // TODO(adam): Check Windows 8 compatability
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        // TODO(adam): Diagnostic
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }

    if (!XInputLibrary)
    {
        // TODO(adam): Diagnostic
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
    Buffer->BytesPerPixel = BytesPerPixel;

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
/* * Message callbacks
 */
internal LRESULT CALLBACK Win32MainWindowCallback(HWND   Window,
                                                  UINT   Message,
                                                  WPARAM WParam,
                                                  LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message)
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
            Assert(!"Keyboard input came in thru a non-dispatch message!");
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
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

/*
 * Clear sound buffer
 */
internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0)))
    {
        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0;
             ByteIndex < Region1Size;
             ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0;
             ByteIndex < Region2Size;
             ++ByteIndex)
        {
            *DestSample++ = 0;
        }
    }
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size,
                                  Region2, Region2Size);
}

/*
 * Fill buffer ahead of play cursor
 */
internal void Win32FillSoundBuffer(win32_sound_output       *SoundOutput,
                                   DWORD                     ByteToLock,
                                   DWORD                     BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0)))
    {
        // TODO(adam): Collapse these loops
        // TODO(adam): assert Region1Size/Region2Size is valid

        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region1SampleCount;
             ++SampleIndex)
        {
            *DestSample++ = *SourceSample++; // L
            *DestSample++ = *SourceSample++; // R
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0;
             SampleIndex < Region2SampleCount;
             ++SampleIndex)
        {
            *DestSample++ = *SourceSample++; // L
            *DestSample++ = *SourceSample++; // R
            ++SoundOutput->RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size,
                                      Region2, Region2Size);
    }
}

/*
 * Handle input messages
 */
internal void Win32ProcessKeyboardMessage(game_button_state *NewState,
                                          bool32             IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void Win32ProcessXInputDigitalButton(DWORD              XInputButtonState,
                                              game_button_state *OldState,
                                              DWORD              ButtonBit,
                                              game_button_state *NewState)
{
    NewState->EndedDown = (XInputButtonState & ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value,
                                             SHORT DeadZoneThreshold)
{
    real32 Result = 0;

    if (Value < -DeadZoneThreshold)
    {
        Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);
    }
    else if (Value > DeadZoneThreshold)
    {
        Result = (real32)(Value - DeadZoneThreshold) / (32767.0f + DeadZoneThreshold);
    }

    return Result;
}

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
    MSG Message;

    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown  = ((Message.lParam & (1 << 31)) == 0);
                if (WasDown != IsDown)
                {
                    if      (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                }
                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if ((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart)
                                    / (real32)GlobalPerfCountFrequency);
    return Result;
}

/*
 * Sound debugging display
 */
internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer,
                                     int                     X,
                                     int                     Top,
                                     int                     Bottom,
                                     uint32                  Color)
{
    uint8 *Pixel = ((uint8 *)BackBuffer->Memory
                    + X*BackBuffer->BytesPerPixel
                    + Top*BackBuffer->Pitch);
    for (int Y = Top;
         Y < Bottom;
         ++Y)
    {
        *(uint32 *)Pixel = Color;
        Pixel += BackBuffer->Pitch;
    }
}


internal void Win32DrawSoundBufferMarker(win32_offscreen_buffer *BackBuffer,
                                         win32_sound_output     *SoundOutput,
                                         real32                  C,
                                         int                     PadX,
                                         int                     Top,
                                         int                     Bottom,
                                         DWORD                   Value,
                                         uint32                  Color)
{
    Assert(Value < (DWORD)SoundOutput->SecondaryBufferSize);
    real32 XReal32 = (C * (real32)Value);
    int X = PadX + (int)XReal32;
// int X = PadX + (int)(C * LastPlayCursor[PlayCursorIndex]);
    Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer  *BackBuffer,
                                    int                      MarkerCount,
                                    win32_debug_time_marker *Markers,
                                    win32_sound_output      *SoundOutput,
                                    real32                   TargetSecondsPerFrame)
{
    // TODO(adam): Draw where we're writing out sound

    int PadX = 16;
    int PadY = 16;

    int Top = PadY;
    int Bottom = BackBuffer->Height - PadY;

    real32 C = (real32)(BackBuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for (int PlayCursorIndex = 0;
         PlayCursorIndex < MarkerCount;
         ++PlayCursorIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[PlayCursorIndex];
        Win32DrawSoundBufferMarker(
            BackBuffer,
            SoundOutput,
            C, PadX, Top, Bottom,
            ThisMarker->PlayCursor,
            0xFFFFFFFF);
        Win32DrawSoundBufferMarker(
            BackBuffer,
            SoundOutput,
            C, PadX, Top, Bottom,
            ThisMarker->WriteCursor,
            0xFFFF0000);
    }
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
     * Get frequency for time calculations
     */
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
// NOTE(adam): Set the Windows scheduler granularity to 1 ms
    // so that our sleep is more granular
    UINT DesiredSchedulerMS = 1;

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

    // TODO(adam): How do we reliably query this on Windows?
#define MonitorRefreshHz 60
#define FramesOfAudioLatency 3
#define GameUpdateHz (MonitorRefreshHz / 2)
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

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
         * If successful, set up sound buffer and start game
         */
        if (Window)
        {
            // NOTE(adam): Since we specified CS_OWNDC, we can just
            // get one DC and use it forever because we are not
            // sharing it
            HDC DeviceContext = GetDC(Window);

            win32_sound_output SoundOutput  = {};
            SoundOutput.SamplesPerSecond    = 48000;
            SoundOutput.BytesPerSample      = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            // TODO(adam): Actually compute this variance and see what the lowest reasonable value is
            SoundOutput.SafetyBytes         = ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample)
                                               / GameUpdateHz / 3);

            // TODO(adam): Get rid of latency sample count
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            bool32 ControllerFound = false;

            // NOTE(adam): This tests play cursor/write cursor update frequency
            // Was 480 samples on laptop
#if 0
            while (GlobalRunning)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                char MsgBuffer[256];
                _snprintf_s(MsgBuffer, sizeof(MsgBuffer), "PC: %u WC: %u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(MsgBuffer);
            }
#endif

            // TODO(adam): Pool with bitmap virtual alloc
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(4);

            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize,
                                                       MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage
                                           + GameMemory.PermanentStorageSize);

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {};

                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds =0;
                bool32 SoundIsValid = false;
                DWORD LastTargetCursor = 0;

                uint64 LastCycleCount = __rdtsc();

                bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

                /*
                 * Start game loop
                 */
                while (GlobalRunning)
                {
                    // TODO(adam): Zeroing macro
                    // TODO(adam): WE can't zero everything because the up/down state
                    // will be wrong!
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;

                    for (int ButtonIndex = 0;
                         ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                         ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    /*
                     * Check for OS messages
                     */
                    Win32ProcessPendingMessages(NewKeyboardController);

                    /*
                     * Poll game controllers
                     */
                    // TODO(adam): Need to not poll disconnected controllers
                    // to avoid XInput frame rate hit on older libraries.
                    // TODO(adam): Should we poll this more frequently?
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
                    }

                    for (DWORD ControllerIndex = 0;
                         ControllerIndex < MaxControllerCount;
                         ++ControllerIndex)
                    {
                        int OurControllerIndex = ControllerIndex + 1;
                        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(adam): This controller is plugged in
                            // TODO(adam): See if ControllerState.dwPacketNumber increments too rapidly

                            NewController->IsConnected = true;

                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            // TODO(adam): This is a square deadzone, check XInput docs
                            // to verify that the deadzone is circular
                            NewController->StickAverageX = Win32ProcessXInputStickValue(
                                Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(
                                Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if ((NewController->StickAverageX != 0.0f) ||
                                (NewController->StickAverageX != 0.0f))
                            {
                                NewController->IsAnalog = true;
                            }

                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                NewController->StickAverageY = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                NewController->StickAverageY = 1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                NewController->StickAverageX = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                NewController->StickAverageX = 1.0f;
                                NewController->IsAnalog = false;
                            }

                            real32 Threshold = 0.5f;
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageX < -Threshold) ? 1 : 0,
                                &OldController->MoveLeft, 1,
                                &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageX > Threshold) ? 1 : 0,
                                &OldController->MoveRight, 1,
                                &NewController->MoveRight);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageY < -Threshold) ? 1 : 0,
                                &OldController->MoveDown, 1,
                                &NewController->MoveDown);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageY > Threshold) ? 1 : 0,
                                &OldController->MoveUp, 1,
                                &NewController->MoveUp);

                            // NOTE(adam): Missing Start and Back
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->ActionDown,
                                XINPUT_GAMEPAD_A,
                                &NewController->ActionDown);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->ActionRight,
                                XINPUT_GAMEPAD_B,
                                &NewController->ActionRight);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->ActionLeft,
                                XINPUT_GAMEPAD_X,
                                &NewController->ActionLeft);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->ActionUp,
                                XINPUT_GAMEPAD_Y,
                                &NewController->ActionUp);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->LeftShoulder,
                                XINPUT_GAMEPAD_LEFT_SHOULDER,
                                &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->RightShoulder,
                                XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                &NewController->RightShoulder);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Start,
                                XINPUT_GAMEPAD_START,
                                &NewController->Start);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Back,
                                XINPUT_GAMEPAD_BACK,
                                &NewController->Back);
                        }
                        else
                        {
                            // NOTE(adam): This controller is not available
                            if (ControllerIndex == 1 && ControllerFound)
                            {
                                // MessageBox(Window, "Fuck, you lost your controller!!!!!", "FUCK!", MB_OK);
                                NewController->IsConnected = false;
                            }
                        }
                    }

                    /*
                     * Get game state to render
                     */
                    // TODO(adam): Sound is wrong now, because we haven't updated it to
                    // go with the new frame rate.
                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.Pitch = GlobalBackBuffer.Pitch;

                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer);

                    /*
                     * Get current state of sound buffer
                     */
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                    {
                        /* NOTE(adam):

                           Here is how sound computation works.

                           We define a safety value that is the number of
                           samples we think our game update loop may vary
                           by (let's say up to 2 ms).

                           When we wake up to write audio, we will look
                           and see what the play cursor position is and
                           we will forecast ahead where we think the play
                           cursor will be on the next frame boundary.

                           We will then look to see if the write cursor is before
                           that. If it is, the target fill position is that frame
                           boundary pluys one frame. This gives us perfect audio
                           sync in the cases of a card that has low enough latency.

                           If the write cursor is _after_ that safety margin,
                           then we will assume we can never sync the audio perfectly,
                           so we will one frame's worth of audio plus the safety
                           margin's worth of guard samples.
                        */

                        if (!SoundIsValid)
                        {
                            SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                        }

                        DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                                            % SoundOutput.SecondaryBufferSize);
                        if (SoundIsValid)
                        {
                            Assert(ByteToLock == LastTargetCursor);
                        }
                        else
                        {
                            SoundIsValid = true;
                        }

                        DWORD ExpectedSoundBytesPerFrame =
                            (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
                        DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

                        DWORD SafeWriteCursor = WriteCursor;
                        if (SafeWriteCursor < PlayCursor)
                        {
                            SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                        }
                        Assert(SafeWriteCursor >= PlayCursor);
                        SafeWriteCursor += SoundOutput.SafetyBytes;
                        bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                        DWORD  TargetCursor = 0;
                        if (AudioCardIsLowLatency)
                        {
                            TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
                        }
                        else
                        {
                            TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
                        }
                        TargetCursor = (TargetCursor & SoundOutput.SecondaryBufferSize);
                        LastTargetCursor = TargetCursor;

                        // TODO(adam): Check this out
                        DWORD  BytesToWrite = 0;
                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }

                        /*
                         * Write sound to buffer
                         */
                        game_sound_output_buffer SoundBuffer = {};
                        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                        // SoundBuffer.SampleCount = SoundBuffer.SamplesPerSecond / 30; // 30 FPS
                        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                        SoundBuffer.Samples = Samples;
                        GameGetSoundSamples(&GameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL
                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                        DWORD UnwrappedWriteCursor = WriteCursor;
                        if (WriteCursor < PlayCursor)
                        {
                            UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                        }
                        AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                        AudioLatencySeconds = ((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample
                                                      / (real32)SoundOutput.SamplesPerSecond);

                        char MsgBuffer[256];
                        _snprintf_s(MsgBuffer, sizeof(MsgBuffer),
                                    "BTL: %u TC: %u PC: %u WC: %u DELTA: %u LOWLAT: %d LAT: %.6f\n",
                                    ByteToLock, TargetCursor, PlayCursor,
                                    WriteCursor, AudioLatencyBytes, AudioCardIsLowLatency, AudioLatencySeconds);
                        OutputDebugStringA(MsgBuffer);
#endif
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }
                    else
                    {
                        SoundIsValid = false;
                    }

                    /*
                     * Check frame rate
                     */
                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                    // TODO(adam): NOT TESTED YET! PROBABLY BUGGY!!!!!
                    real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    char SPFMsgBuffer[256];
                    _snprintf_s(SPFMsgBuffer, sizeof(SPFMsgBuffer), "BEFORE SLEEP: %.2f\n", 1000.0f * WorkSecondsElapsed);
                    OutputDebugStringA(SPFMsgBuffer);
                    if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        if (SleepIsGranular)
                        {
                            DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                            if (SleepMS > 0)
                            {
                                Sleep(SleepMS);
                            }
                        }

                        // TODO(adam): vvv This is failing at the moment
                        real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        if (TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // TODO(adam): Log missed sleep
                        }
                        char MsgBuffer[256];
                        _snprintf_s(MsgBuffer, sizeof(MsgBuffer), "AFTER SLEEP: %.2f\n", 1000.0f * TestSecondsElapsedForFrame);
                        OutputDebugStringA(MsgBuffer);
                        while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        }
                    }
                    else
                    {
                        // TODO(adam): MISSED FRAME RATE!
                        // TODO(adam): Logging
                    }

                    /*
                     * Render frame
                     */
                    win32_window_dimensions Dimension = Win32GetWindowDimensions(Window);
#if HANDMADE_INTERNAL
                    Win32DebugSyncDisplay(
                        &GlobalBackBuffer,
                        ArrayCount(DebugTimeMarkers),
                        DebugTimeMarkers,
                        &SoundOutput,
                        TargetSecondsPerFrame);
#endif
                    Win32DisplayBufferInWindow(
                        DeviceContext,
                        &GlobalBackBuffer,
                        Dimension.Width,
                        Dimension.Height);

                    {
                        real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        char MsgBuffer[256];
                        _snprintf_s(MsgBuffer, sizeof(MsgBuffer), "AFTER RENDER: %.2f\n", 1000.0f * TestSecondsElapsedForFrame);
                        OutputDebugStringA(MsgBuffer);
                    }
#if HANDMADE_INTERNAL
                    /*
                     * NOTE(adam): DEBUGGING
                     */
                    {
                        DWORD PlayCursor, WriteCursor;
                        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                        {
                            Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex++];
                            if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
                            {
                                DebugTimeMarkerIndex = 0;
                            }

                            Marker->PlayCursor = PlayCursor;
                            Marker->WriteCursor = WriteCursor;
                        }
                    }
#endif

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    // TODO(adam): Should I clear these here?


                    LARGE_INTEGER EndCounter = Win32GetWallClock();
                    real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
                    LastCounter = EndCounter;

                    uint64 EndCycleCount = __rdtsc();
                    uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    LastCycleCount = EndCycleCount;

                    real32 FPS = 0.0f;
                    real32 MCPF = (real32)CyclesElapsed / 1e6f;

                    char MsgBuffer[256];
                    _snprintf_s(MsgBuffer, sizeof(MsgBuffer), "%.2f ms/f, FPS: %.2f, %.2f Mc/f\n", MSPerFrame, FPS, MCPF);
                    OutputDebugStringA(MsgBuffer);
                }
            }
            // Failed to allocate memory
            else
            {
                // TODO(adam): Logging
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

    timeEndPeriod(DesiredSchedulerMS);

    return 0;
}
