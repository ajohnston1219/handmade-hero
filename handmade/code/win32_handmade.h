#if !defined(WIN32_HANDMADE_H)

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

struct win32_sound_output
{
    int    SamplesPerSecond;
    uint32 RunningSampleIndex;
    int    BytesPerSample;
    int    SecondaryBufferSize;
    int    LatencySampleCount;
};

#define WIN32_HANDMADE_H
#endif