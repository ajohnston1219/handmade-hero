#include "handmade.h"
#include <math.h>

/*
 * Fill buffer ahead of play cursor
 */
internal void OutputGameSound(game_sound_output_buffer *SoundBuffer,
                              int                       ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;

    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->SampleCount;
         ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue; // L
        *SampleOut++ = SampleValue; // R

        tSine += 2.0f * Pi32  / (real32)WavePeriod;
    }
}


/*
 * Paint to bitmap
 */
internal void RenderWeirdGradient(game_offscreen_buffer *Buffer,
                                  int                    XOffset,
                                  int                    YOffset,
                                  int                    RedShift)
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
            uint8 Blue = X + XOffset;
            uint8 Green = Y + YOffset;
            uint8 Red = Blue + Green + RedShift;

            *Pixel++ = (uint32)((Red << 16) | (Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(game_offscreen_buffer    *Buffer,
                                  int                       XOffset,
                                  int                       YOffset,
                                  int                       RedShift,
                                  game_sound_output_buffer *SoundBuffer,
                                  int                       ToneHz)
{
    // TODO(adam): Allow sample offsets here for more robust platform options
    OutputGameSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, XOffset, YOffset, RedShift);
}

void MainLoop()
{
}
