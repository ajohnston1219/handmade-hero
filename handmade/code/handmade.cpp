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

        tSine += 2.0f * (real32)Pi32  / (real32)WavePeriod;
    }
}


/*
 * Paint to bitmap
 */
internal void RenderWeirdGradient(game_offscreen_buffer *Buffer,
                                  int                    BlueOffset,
                                  int                    GreenOffset,
                                  int                    RedOffset)
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
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);
            uint8 Red = (uint8)(Blue + Green + RedOffset);

            *Pixel++ = (uint32)((Red << 16) | (Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void GameUpdateAndRender(game_memory              *Memory,
                                  game_input               *Input,
                                  game_offscreen_buffer    *Buffer,
                                  game_sound_output_buffer *SoundBuffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        char *Filename = __FILE__;

        debug_read_file_result File = DEBUGPlatformReadEntireFile(Filename);
        if (File.Contents)
        {
            DEBUGPlatformWriteEntireFile("test.out", File.ContentSize, File.Contents);
            DEBUGPlatformFreeFileMemory(File.Contents);
        }

        // NOTE(adam): Game memory is initialized to zero
        GameState->ToneHz = 256;
        // TODO(adam): May be better to do in platform layer
        Memory->IsInitialized = true;
    }

    game_controller_input *Input0 = &Input->Controllers[0];
    if (Input0->IsAnalog)
    {
        // NOTE(adam): Use analog movement tuning
        GameState->ToneHz = 256 + (int)(128.0f * Input0->EndX);
        GameState->BlueOffset += (int)(4.0f * Input0->EndY);
    }
    else
    {
        // NOTE(adam): Use digital movement tuning
    }

    // Input.AButtonEndedDown;
    // Input.AButtonHalfTransitionCount;
    if (Input0->Down.EndedDown)
    {
        GameState->GreenOffset += 1;
        GameState->RedOffset -= 1;
    }

    // TODO(adam): Allow sample offsets here for more robust platform options
    OutputGameSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(
        Buffer,
        GameState->BlueOffset,
        GameState->GreenOffset,
        GameState->RedOffset);
}

void MainLoop()
{
}
