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
                                  game_offscreen_buffer    *Buffer)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0])
           == (ArrayCount(Input->Controllers[0].Buttons)));
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

    for (int ControllerIndex = 0;
         ControllerIndex < ArrayCount(Input->Controllers);
         ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE(adam): Use analog movement tuning
            GameState->ToneHz = 256 + (int)(128.0f * Controller->StickAverageX);
            GameState->BlueOffset += (int)(4.0f * Controller->StickAverageY);
        }
        else
        {
            // NOTE(adam): Use digital movement tuning
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }

            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset += 1;
            }
        }

        // Input.AButtonEndedDown;
        // Input.AButtonHalfTransitionCount;
        if (Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset += 1;
            GameState->RedOffset -= 1;
        }
    }
    RenderWeirdGradient(
        Buffer,
        GameState->BlueOffset,
        GameState->GreenOffset,
        GameState->RedOffset);
}

internal void GameGetSoundSamples(game_memory              *Memory,
                                  game_sound_output_buffer *SoundBuffer)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    OutputGameSound(SoundBuffer, GameState->ToneHz);
}
