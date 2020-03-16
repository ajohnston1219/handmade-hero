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

#define Pi32 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342

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
            uint32 VKCode = (uint32)WParam;
            bool32 WasDown = ((LParam & (1 << 30)) != 0);
            bool32 IsDown  = ((LParam & (1 << 31)) == 0);
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

internal void Win32ProcessXInputDigitalButton(DWORD              XInputButtonState,
                                              game_button_state *OldState,
                                              DWORD              ButtonBit,
                                              game_button_state *NewState)
{
    NewState->EndedDown = (XInputButtonState & ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
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
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

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
         * If successful, set up sound buffer and start game
         */
        if (Window)
        {
            // NOTE(adam): Since we specified CS_OWNDC, we can just
            // get one DC and use it forever because we are not
            // sharing it
            HDC DeviceContext = GetDC(Window);

            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond      = 48000;
            // SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample        = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize   = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount    = SoundOutput.SamplesPerSecond / 15;

            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            bool32 ControllerFound = false;

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
            GameMemory.TransientStorageSize = Gigabytes((uint64)4);

            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize,
                                                       MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage
                                           + GameMemory.PermanentStorageSize);

            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                LARGE_INTEGER LastCounter;
                QueryPerformanceCounter(&LastCounter);
                uint64 LastCycleCount = __rdtsc();

                /*
                 * Start game loop
                 */
                while (GlobalRunning)
                {
                    /*
                     * Check for OS messages
                     */
                    MSG Message;
                    // vvvvv Blocks vvvvv
                    // BOOL MessageResult = GetMessageA(&Message, 0, 0, 0);

                    game_input Input[2] = {};
                    game_input *NewInput = &Input[0];
                    game_input *OldInput = &Input[1];

                    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                    {
                        if (Message.message == WM_QUIT)
                        {
                            GlobalRunning = false;
                        }
                        TranslateMessage(&Message);
                        DispatchMessageA(&Message);
                    }

                    /*
                     * Poll game controllers
                     */
                    // TODO(adam): Should we poll this more frequently?
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > ArrayCount(NewInput->Controllers))
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers);
                    }

                    for (DWORD ControllerIndex = 0;
                         ControllerIndex < XUSER_MAX_COUNT;
                         ++ControllerIndex)
                    {
                        game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
                        game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];
                        NewController->IsAnalog = true;

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(adam): This controller is plugged in
                            // TODO(adam): See if ControllerState.dwPacketNumber increments too rapidly

                            ControllerFound = true;

                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            bool32 Up     = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            bool32 Down   = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            bool32 Left   = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            bool32 Right  = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                            int16  StickX = Pad->sThumbLX;
                            int16  StickY = Pad->sThumbLY;

                            NewController->StartX = OldController->EndX;
                            NewController->StartY = OldController->EndY;

                            // TODO(adam): Handle deadzone with
                            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE   7849
                            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE  8689

                            // TODO(adam): Min/Max macros!!!
                            // TODO(adam): Handle deadzone with
                            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE   7849
                            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE  8689

                            // TODO(adam): Collapse to single function
                            real32 X;
                            if (Pad->sThumbLX < 0)
                            {
                                X = (real32)Pad->sThumbLX / 32768.0f;
                            }
                            else
                            {
                                X = (real32)Pad->sThumbLX / 32767.0f;
                            }
                            NewController->MinX = NewController->MaxX = NewController->EndX = X;

                            real32 Y;
                            if (Pad->sThumbLX < 0)
                            {
                                Y = (real32)Pad->sThumbLY / 32768.0f;
                            }
                            else
                            {
                                Y = (real32)Pad->sThumbLY / 32767.0f;
                            }
                            NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                            // NOTE(adam): Missing Start and Back

                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Down,
                                XINPUT_GAMEPAD_A,
                                &NewController->Down);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Right,
                                XINPUT_GAMEPAD_B,
                                &NewController->Right);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Left,
                                XINPUT_GAMEPAD_X,
                                &NewController->Left);
                            Win32ProcessXInputDigitalButton(
                                Pad->wButtons,
                                &OldController->Up,
                                XINPUT_GAMEPAD_Y,
                                &NewController->Up);
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

                    /*
                     * Get current state of sound buffer
                     */
                    DWORD  ByteToLock   = 0;
                    DWORD  TargetCursor = 0;
                    DWORD  BytesToWrite = 0;
                    DWORD  PlayCursor   = 0;
                    DWORD  WriteCursor  = 0;
                    bool32 SoundIsValid = false;
                    // TODO(adam): Tighten up sound logic so that we know where we should be
                    // writing to and can anticipate the time spent in the game update.
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {
                        ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
                            % SoundOutput.SecondaryBufferSize;
                        TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample))
                            % SoundOutput.SecondaryBufferSize;

                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }

                        SoundIsValid = true;
                    }

                    /*
                     * Get game state to render
                     */
                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    // SoundBuffer.SampleCount = SoundBuffer.SamplesPerSecond / 30; // 30 FPS
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;

                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackBuffer.Memory;
                    Buffer.Width = GlobalBackBuffer.Width;
                    Buffer.Height = GlobalBackBuffer.Height;
                    Buffer.Pitch = GlobalBackBuffer.Pitch;

                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

                    /*
                     * Write sound to buffer
                     */
                    if (SoundIsValid)
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }

                    /*
                     * Render frame
                     */
                    win32_window_dimensions Dimension = Win32GetWindowDimensions(Window);
                    Win32DisplayBufferInWindow(
                        DeviceContext,
                        &GlobalBackBuffer,
                        Dimension.Width,
                        Dimension.Height);

                    /*
                     * Profiling
                     */
                    uint64 EndCycleCount = __rdtsc();

                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);

                    uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                    int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                    real32 MSPerFrame = 1000.0f * (real32)CounterElapsed / (real32)PerfCountFrequency;
                    real32 FramesPerSecond =  1000.0f / MSPerFrame;
                    real32 MCPF = CyclesElapsed / 1e6f;

#if 0
                    char MsgBuffer[256];
                    sprintf(MsgBuffer, "%.2f ms/f, FPS: %.2f, %.2f Mc/f\n", MSPerFrame, FramesPerSecond, MCPF);
                    OutputDebugStringA(MsgBuffer);
#endif

                    LastCounter = EndCounter;
                    LastCycleCount = EndCycleCount;

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    // TODO(adam): Should I clear these here?
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

    return 0;
}
