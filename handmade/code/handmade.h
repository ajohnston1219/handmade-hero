#if !defined(HANDMADE_H)

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
    game_controller_input Controllers[4];
};

void GameUpdateAndRender(game_input               *Input,
                         game_offscreen_buffer    *Buffer,
                         game_sound_output_buffer *SoundBuffer);

void *PlatformLoadFile(char *Filename);

#define HANDMADE_H
#endif
