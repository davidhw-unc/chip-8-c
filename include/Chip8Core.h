#ifndef CHIP_8_CORE_H
#define CHIP_8_CORE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct Chip8Proc {

    /*** Registers ***/
    // 16 general-purpose 8-bit registers
    uint8_t V[16];
    // 16-bit register primarily for storing addresses
    uint16_t I;
    // Special 8-bit registers for the delay & sound timers
    // When != 0; these decrease at 60Hz
    uint8_t D, S;
    // Flag registers (extra registers from Super)
    uint8_t FR[8];

    /*** Pseudo-Registers (Inaccessible Registers) ***/
    // Program counter
    int16_t PC;
    // Stack counter
    int8_t SC;

    /*** Stack ***/
    // Array of 16 16-bit values
    uint16_t stack[16];

    /*** Memory ***/
    uint8_t ram[4096];

    /*** Screen ***/
    // 128x64 bool array (row-major)
    bool screen[64 * 128];

    /*** External Interaction ***/
    void (*sendScreen)(bool *screen);
    void (*setSound)(bool isPlaying, struct Chip8Proc *self);

    /*** Mode Flag ***/
    bool superMode;
    bool largeScreen;

} Chip8Proc;

/*
 * Create a new Chip8Proc, copy program into the processor's memory, and
 * store the provided funciton pointers for output use
 */
Chip8Proc Chip8_init(uint8_t *program,
        size_t progSize,
        void (*sendScreen)(bool *screen),
        void (*setSound)(bool isPlaying, Chip8Proc *self),
        bool superMode);

/*
 * Advance the Chip8 processor by one step.
 * Does not decrement timers.
 * Returns false if the processor has exited (00FD), true otherwise.
 */
bool Chip8_advance(Chip8Proc *self);

/*
 * Run the Chip8 processor
 */
void Chip8_run(Chip8Proc *self);

#endif
