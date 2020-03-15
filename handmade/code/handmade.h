#if !defined(HANDMADE_H)

/*
  NOTE(adam):

  HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  HANDMADE_SLOW:
    0 - No slow code allowed!
    1 - Slow code welcome
 */

// Write to the nullptr bcynot
#if HANDMADE_SLOW
// TODO(adam): Complete assertion macro
#define Assert(Expression) if (!(Expression)) { *(int *)0 = 0; }
#else
#define Assert(Expression)
#endif

// TODO(adam): Should these always use 64 bit?
#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
/*
 * TODO(adam): swap, min, max ... macros??
 */

/*
 * TODO(adam): Services that the platform layer provides to the game.
 */

/*
 * NOTE(adam): Services that the game provides to the platform layer.
 */

/*
 * NEEDS:
 * - Timing
 * - Controller/Keyboard Input
 * - Bitmat buffer
 * - Sound buffer
 */

// TODO(adam): In the future, rendering _specifically_ will become a three-teired abstraction!!!
struct game_offscreen_buffer
{
    // NOTE(adam): Pixels are always 32-bits widw,
    // Memory Order: BB GG RR XX
    void *Memory;
    int   Width;
    int   Height;
    int   Pitch;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    // TODO(adam): Insert clock values here.
    game_controller_input Controllers[4];
};

struct game_memory
{
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void *PermanentStorage; // NOTE(adam): cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; // NOTE(adam): cleared to zero at startup
};


struct game_state
{
    int ToneHz;
    int RedOffset;
    int GreenOffset;
    int BlueOffset;
};

void GameUpdateAndRender(game_memory              *Memory,
                         game_input               *Input,
                         game_offscreen_buffer    *Buffer,
                         game_sound_output_buffer *SoundBuffer);

void *PlatformLoadFile(char *Filename);

#define HANDMADE_H
#endif