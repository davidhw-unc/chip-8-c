#include <stdlib.h>
#include <stdio.h>

#include "Chip8Core.h"
#include "Guards.h"

void printScreen(bool *screen);

int main() {
    uint8_t maze[38] = { // Maze (alt) [David Winter, 199x]
        0x60, 0x00, 0x61, 0x00, 0xA2, 0x22, 0xC2, 0x01,
        0x32, 0x01, 0xA2, 0x1E, 0xD0, 0x14, 0x70, 0x04,
        0x30, 0x40, 0x12, 0x04, 0x60, 0x00, 0x71, 0x04,
        0x31, 0x20, 0x12, 0x04, 0x12, 0x1C, 0x80, 0x40,
        0x20, 0x10, 0x20, 0x40, 0x80, 0x10
    };

    uint8_t testProg[50] = {
        0x22, 0x08, 0x51, 0x20, 0x12, 0x04, 0x12, 0x06,
        0x61, 0x1E, 0x62, 0xFE, 0x00, 0xEE
    };

    Chip8Proc *proc = malloc(sizeof(Chip8Proc));
    OOM_GUARD(proc, __FILE__, __LINE__);
    *proc = Chip8_init(testProg, 50, printScreen, NULL, false);

    for (int i = 0; i < 10; ++i) {
        printf("PC: %03X\n", proc->PC);
        Chip8_advance(proc);
    }

    // Cleanup
    Chip8_end(proc);
    free(proc);
    proc = NULL;
    return EXIT_SUCCESS;
}

void printScreen(bool *screen) {
    for (int i = 0; i < 30; ++i) { putchar('\n'); }
    putchar('+');
    for (int i = 0; i < 128; ++i) { putchar('-'); }
    putchar('+');
    putchar('\n');
    for (int c = 0; c < 128; ++c) {
        putchar('|');
        for (int r = 0; r < 64; ++r) {
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
