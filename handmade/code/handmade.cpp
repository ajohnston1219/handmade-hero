#include "handmade.h"

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

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset, int RedShift)
{
    RenderWeirdGradient(Buffer, XOffset, YOffset, RedShift);
}

void MainLoop()
{
}
