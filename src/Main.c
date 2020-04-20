#include <stdlib.h>
#include <stdio.h>

#include "Chip8Core.h"
#include "Guards.h"

void printScreen(bool *screen);
void printScreenCompact(bool *screen);

int main() {
    uint8_t maze[64] = { // Maze (alt) [David Winter, 199x]
        0x60, 0x00, 0x61, 0x00, 0xA2, 0x22, 0xC2, 0x01,
        0x32, 0x01, 0xA2, 0x1E, 0xD0, 0x14, 0x70, 0x04,
        0x30, 0x40, 0x12, 0x04, 0x60, 0x00, 0x71, 0x04,
        0x31, 0x20, 0x12, 0x04, 0x12, 0x1C, 0x80, 0x40,
        0x20, 0x10, 0x20, 0x40, 0x80, 0x10
    };

    uint8_t writeText[64] = {
        // Set V7 to 0x00
        0x67, 0x00,
        // Start V0 (x)
        0x60, 0x02,
        // Start V1 (y)
        0x61, 0x01,
        // Set I to addr of sprite for char in V7
        0xF7, 0x29, 
        // Draw the sprite
        0xD0, 0x15,
        // Add 5 to V0 (x)
        0x70, 0x05,
        // Add 1 to V7
        0x77, 0x01,
        // If V0 (x) == 0x3E, don't go back yet
        0x30, 0x3E,
        // Go back to the fetch sprite instruction
        0x12, 0x06,
        // Add 6 to V1 (y)
        0x71, 0x06,
        // Reset V0 (x)
        0x60, 0x02,
        // If V1 (y) == 0x1F, don't go back
        0x31, 0x1F,
        // Go back to the fetch sprite instruction
        0x12, 0x06,
        // Test scrolling
        0x00, 0xFC,
        0x00, 0xFC,
        0x00, 0xFB,
        0x00, 0xCc,
        // Exit
        0x00, 0xFD
    };

    uint8_t largeScreenDraw[64] = {
        // Enable largeScreen mode
        0x00, 0xFF,
        // Skip sprite data
        0x12, 0x24,
        // Sprite data
        0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        // Set I to addr of sprite data
        0xA2, 0x04,
        // Set V0 to x
        0x60, 0x00,
        // Set V1 to y
        0x61, 0x00,
        // Draw 8x15
        0xD0, 0x1F,
        // Set V1 to next x
        0x60, 0x00,
        // Set V2 to next y
        0x61, 0x20,
        // Draw 16x16
        0xD0, 0x10,
        // Exit
        0x00, 0xFD
    };

    Chip8Proc *proc = malloc(sizeof(Chip8Proc));
    OOM_GUARD(proc, __FILE__, __LINE__);
    *proc = Chip8_init(largeScreenDraw, 64, printScreenCompact, NULL, false);

    while (Chip8_advance(proc));
    printf("Done.\n");

    // Cleanup
    free(proc);
    proc = NULL;
    return EXIT_SUCCESS;
}

void printScreenCompact(bool *screen) {
    printf("\xE2\x96\x88");
    for (int i = 0; i < 130; ++i) { printf("\xE2\x96\x80"); }
    printf("\xE2\x96\x88\n");
    for (int r = 0; r < 64; r += 2) {
        printf("\xE2\x96\x88 ");
        for (int c = 0; c < 128; ++c) {
            if (screen[r * 128 + c]) {
                if (screen[(r + 1) * 128 + c]) {
                    printf("\xE2\x96\x88");
                } else {
                    printf("\xE2\x96\x80");
                }
            } else {
                if (screen[(r + 1) * 128 + c ]) {
                    printf("\xE2\x96\x84");
                } else {
                    printf(" ");
                }
            }
        }
        printf(" \xE2\x96\x88\n");
    }
    printf("\xE2\x96\x88");
    for (int i = 0; i < 130; ++i) { printf("\xE2\x96\x84"); }
    printf("\xE2\x96\x88\n");
    putchar('\n');
}

void printScreen(bool *screen) {
    putchar('+');
    for (int i = 0; i < 128; ++i) { putchar('-'); }
    putchar('+');
    putchar('\n');
    for (int r = 0; r < 64; ++r) {
        putchar('|');
        for (int c = 0; c < 128; ++c) {
            putchar(screen[r * 128 + c] ? '#' : ' ');
        }
        putchar('|');
        putchar('\n');
    }
    putchar('+');
    for (int i = 0; i < 128; ++i) { putchar('-'); }
    putchar('+');
    putchar('\n');
}
