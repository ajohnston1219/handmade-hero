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
                                  int                    BlueOffset,
                                  int                    GreenOffset,
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
            uint8 Blue = X + BlueOffset;
            uint8 Green = Y + GreenOffset;
            uint8 Red = Blue + Green + RedShift;

            *Pixel++ = (uint32)((Red << 16) | (Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(game_input               *Input,
                                  game_offscreen_buffer    *Buffer,
                                  game_sound_output_buffer *SoundBuffer)
{
    local_persist int BlueOffset = 0;
    local_persist int GreenOffset = 0;
    local_persist int RedShift = 0;
    local_persist int ToneHz = 256;

    game_controller_input *Input0 = &Input->Controllers[0];
    if (Input0->IsAnalog)
    {
        // NOTE(adam): Use analog movement tuning
        ToneHz = 256 + (int)(128.0f * (Input0->EndX));
        BlueOffset += (int)(4.0f * (Input0->EndY));
        char MsgBuffer[256];
        wsprintfA(MsgBuffer, "Blue Offset = %d\n", BlueOffset);
        OutputDebugStringA(MsgBuffer);
    }
    else
    {
        // NOTE(adam): Use digital movement tuning
    }

    // Input.AButtonEndedDown;
    // Input.AButtonHalfTransitionCount;
    if (Input0->Down.EndedDown)
    {
        GreenOffset += 1;
        RedShift -= 1;
    }

    // TODO(adam): Allow sample offsets here for more robust platform options
    OutputGameSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset, RedShift);
}

void MainLoop()
{
}
