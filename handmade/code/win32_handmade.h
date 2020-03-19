#if !defined(WIN32_HANDMADE_H)

struct win32_offscreen_buffer
{
    BITMAPINFO  Info;
    void       *Memory;
    int         Width;
    int         Height;
    int         Pitch;
    int         BytesPerPixel;
};

struct win32_window_dimensions
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int    SamplesPerSecond;
    uint32 RunningSampleIndex;
    int    BytesPerSample;
    int    SecondaryBufferSize;
    int    LatencySampleCount;
    DWORD  SafetyBytes;
    // TODO(adam): BytesPerSecond
    // TODO(adam): Should running sample index be in bytes?
};

struct win32_debug_time_marker
{
    DWORD PlayCursor;
    DWORD WriteCursor;
};

#define WIN32_HANDMADE_H
#endif
