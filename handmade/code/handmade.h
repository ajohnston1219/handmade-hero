#if !defined(HANDMADE_H)

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
struct game_offscreen_buffer
{
    // NOTE(adam): Pixels are always 32-bits widw,
    // Memory Order: BB GG RR XX
    void       *Memory;
    int         Width;
    int         Height;
    int         Pitch;
};
void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset, int RedShift);

void *PlatformLoadFile(char *Filename);

#define HANDMADE_H
#endif
