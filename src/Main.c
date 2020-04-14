#include <stdlib.h>
#include <stdio.h>

#include "Chip8Core.h"
#include "Guards.h"

int main() {
    uint8_t testProg[38] = { // Maze (alt) [David Winter, 199x]
        0x60, 0x00, 0xFF, 0x00, 0xA2, 0x22, 0xC2, 0x01,
        0x32, 0x01, 0xA2, 0x1E, 0xD0, 0x14, 0x70, 0x04,
        0x30, 0x40, 0x12, 0x04, 0x60, 0x00, 0x71, 0x04,
        0x31, 0x20, 0x12, 0x04, 0x12, 0x1C, 0x80, 0x40,
        0x20, 0x10, 0x20, 0x40, 0x80, 0x10
    };

    Chip8Proc *proc = malloc(sizeof(Chip8Proc));
    OOM_GUARD(proc, __FILE__, __LINE__);
    *proc = Chip8_init(testProg, 4, NULL, NULL, false);

    for (int i = 0; i < 38; ++i) {
        printf("PC: %3X\n", proc->PC);
        Chip8_advance(proc);
    }

    // Cleanup
    Chip8_end(proc);
    free(proc);
    proc = NULL;
    return EXIT_SUCCESS;
}
