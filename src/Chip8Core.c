#include <stdio.h>    
#include <string.h>

#include "Chip8Core.h"
#include "Guards.h"

#define FONT_5_START  0x000
#define FONT_10_START 0x080
#define PROG_START    0x200

static uint8_t font5[80], font10[100];

Chip8Proc Chip8_init(uint8_t *program,
        size_t progSize,
        void (*sendScreen)(bool *screen),
        void (*setSound)(bool isPlaying, Chip8Proc *self),
        bool superMode) {
    // Init the new Chip8Proc
    Chip8Proc proc = {
        .SC = -1,
        .sendScreen = sendScreen,
        .setSound = setSound,
        .superMode = superMode
    };
    // Init the proc's ram
    proc.ram = calloc(4096, sizeof(uint16_t));
    OOM_GUARD(proc.ram, __FILE__, __LINE__);
    memcpy(proc.ram + FONT_5_START, font5, sizeof(font5));
    memcpy(proc.ram + FONT_10_START, font10, sizeof(font10));
    memcpy(proc.ram + PROG_START, program, progSize);
    proc.PC = 0x200;
    // Init the screen buffer
    proc.screen = calloc(128 * 64, sizeof(bool));
    OOM_GUARD(proc.screen, __FILE__, __LINE__);
    return proc;
}

void Chip8_end(Chip8Proc *self) {
    // Free ram
    free(self->ram);
    self->ram = NULL;
    // Free screen
    free(self->screen);
    self->screen = NULL;
}

void Chip8_advance(Chip8Proc *self) {

    // NOTE: see https://github.com/Chromatophore/HP48-Superchip for
    // differences between CHIP-8 and SUPERCHIP-48

    bool normInc = true, validInst = false;

    // Get instruction broken into nibbles
    uint8_t op12 = self->ram[self->PC], op34 = self->ram[self->PC + 1];
    uint8_t op1 = op12 >> 4,
            op2 = op12 & 0x0F,
            op3 = op34 >> 4,
            op4 = op34 & 0x0F;

    // Switch to identify opcodes
    switch (op1) {
        case 0x0:
            // 0nnn: (IGNORED) Call RCA 1802 program at address nnn
            if (op12 != 0x00) {
                break;
            }
            if (op3 == 0xC) { // 00Cn: Scroll display n lines down
                validInst = true;
                // TODO 00Cn
                break;
            }
            switch (op34) {
                case 0xE0: // 00E0: Clear the screen
                    validInst = true;
                    for (int i = 0; i < 128; ++i) { self->screen[i] = 0; }
                    break;
                case 0xEE: // 00EE: Return from subroutine
                    validInst = true;
                    if (self->SC >= 0) {
                        self->PC = self->stack[self->SC--];
                    } else {
                        fprintf(stderr, "%03X - Aborting - Attempted to "
                                "leave subroutine with empty stack\n",
                                self->PC);
                        exit(EXIT_FAILURE);
                    }
                    break;
                case 0xFB: // 00FB: Scroll 4 small pixels right
                    validInst = true;
                    // TODO 00FB
                    break;
                case 0xFC: // 00FC: Scroll 4 small pixels left
                    validInst = true;
                    // TODO 00FC
                    break;
                case 0xFD: // 00FD: Exit interpreter
                    validInst = true;
                    // TODO 00FD
                    break;
                case 0xFE: // 00FE: Switch to lores
                    validInst = true;
                    self->largeScreen = true;
                    break;
                case 0xFF: // 00FF: Switch to hires
                    validInst = true;
                    self->largeScreen = false;
                    break;
            }
            break;
        case 0x1: // 1nnn: Jump to address nnn
            validInst = true;
            self->PC = op2 << 8 | op34;
            normInc = false;
            break;
        case 0x2: // 2nnn: Call subroutine at address nnn
            validInst = true;
            if (self->SC++ < 15) {
                self->stack[self->SC] = self->PC;
                self->PC = op2 << 8 | op34;
                normInc = false;
            } else {
                fprintf(stderr, "%03X - Aborting - Stack overflow\n", self->PC);
                exit(EXIT_FAILURE);
            }
            break;
        case 0x3: // 3xnn: Skip next instruction if Vx == nn
            validInst = true;
            if (self->V[op2] == op34) {
                self->PC += 2;
            }
            break;
        case 0x4: // 4xnn: Skip next instruction if Vx != nn
            validInst = true;
            if (self->V[op2] != op34) {
                self->PC += 2;
            }
            break;
        case 0x5:
            if (op4 == 0) { // 5xy0: Skip next instruction if Vx == Vy
                validInst = true;
                if (self->V[op2] == self->V[op3]) {
                    self->PC += 2;
                }
            }
            break;
        case 0x6: // 6xnn: Set Vx to nn
            validInst = true;
            self->V[op2] = op34;
            break;
        case 0x7: // 7xnn: Add nn to Vx (no carry flag change)
            validInst = true;
            self->V[op2] += op34;
            break;
        case 0x8:
            switch (op4) {
                case 0x0: // 8xy0: Set Vx to Vy
                    validInst = true;
                    self->V[op2] = self->V[op3];
                    break;
                case 0x1: // 8xy1: Set Vx to Vx | Vy
                    validInst = true;
                    self->V[op2] |= self->V[op3];
                    break;
                case 0x2: // 8xy2: Set Vx to Vx & Vy
                    validInst = true;
                    self->V[op2] &= self->V[op3];
                    break;
                case 0x3: // 8xy3: Set Vx to Vx ^ Vy
                    validInst = true;
                    self->V[op2] ^= self->V[op3];
                    break;
                case 0x4: // 8xy4: Set Vx to Vx + Vy, Vf to carry
                    validInst = true;
                    int sum = self->V[op2] + self->V[op3];
                    self->V[0xF] = sum >= 255;
                    self->V[op2] = sum;
                    break;
                case 0x5: // 8xy5: Set Vx to Vx - Vy, Vf to !borrow
                    validInst = true;
                    // TODO 8xy5
                    break;
                case 0x6: // 8xy6: Set Vx to (superMode ? Vx : Vy) >> 1, Vf to lost bit
                    validInst = true;
                    // TODO 8xy6
                    break;
                case 0x7: // 8xy7: Set Vx to Vy - Vx, Vf to !borrow
                    validInst = true;
                    // TODO 8xy7
                    break;
                case 0xE: // 8xy6: Set Vx to (superMode ? Vx : Vy) << 1, Vf to lost bit
                    validInst = true;
                    // TODO 8xyE
                    break;
            }
            break;
        case 0x9:
            if (op4 == 0x0) { // 9xy0: Skip next instruction if Vx != Vy
                validInst = true;
                // TODO 9xy0
            }
            break;
        case 0xA: // Annn: Set I to nnn
            validInst = true;
            // TODO Annn
            break;
        case 0xB: // Bnnn: Set I to nnn + V0 (Super has a quirk, not implemented)
            validInst = true;
            // TODO Bnnn
            break;
        case 0xC: // Cxnn: Set Vx to rand & nn
            validInst = true;
            // TODO Cxnn
            break;
        case 0xD:
            if (op4 == 0x0) { // Dxy0: 16-bit draw in Super, draw nothing otherwise
                validInst = true;
                // TODO Dxy0
            } else { // Dxyn: Draw sprite from ram(I) at (Vx, Vy), set Vf to collision
                validInst = true;
                // TODO Dxyn
            }
            break;
        case 0xE:
            switch (op34) {
                case 0x9E: // Ex9E: Skip next instruction if key Vx is not pressed
                    validInst = true;
                    // TODO Ex9E
                    break;
                case 0xA1: // ExA1: Skip next instruction if key Vx is pressed
                    validInst = true;
                    // TODO ExA1
                    break;
            }
            break;
        case 0xF:
            switch (op34) {
                case 0x07: // Fx07: Set Vx to D
                    validInst = true;
                    // TODO Fx07
                    break;
                case 0x0A: // Fx0A: Wait for keypress, then set Vx to key
                    validInst = true;
                    // TODO Fx0A
                    break;
                case 0x15: // Fx15: Set D to Vx
                    validInst = true;
                    // TODO Fx15
                    break;
                case 0x18: // Fx18: Set S to Vx, call setSound(true, self)
                    validInst = true;
                    // TODO Fx18
                    break;
                case 0x1E: // Fx1E: Set I to I + Vx, Super has an (ignored) quirk
                    validInst = true;
                    // TODO Fx1E
                    break;
                case 0x29: // Fx29: Point I to 5-wide sprite for the hex char in Vx
                    validInst = true;
                    self->I = FONT_5_START + 5 * op2;
                    break;
                case 0x30: // Fx30: Point I to 10-wide sprite for the num char in Vx
                    validInst = true;
                    self->I = FONT_10_START + 10 * op2;
                    break;
                case 0x33: // Fx30: Set I, I+1, I+2 to the decimal digits of Vx
                    validInst = true;
                    // TODO Fx33
                    break;
                case 0x55: // Fx55: Store V0...Vx to ram starting at I
                    validInst = true;
                    // TODO Fx55
                    break;
                case 0x65: // Fx65: Read V0...Vx from ram starting at I
                    validInst = true;
                    // TODO Fx65
                    break;
            }
            // Fx75: (IGNORED) Store V0...Vx to RPL user flags (x < 8, Super only)
            // Fx85: (IGNORED) Read V0...Vx from RPL user flags (x < 8, Super only)
            break;
    }

    // Throw error if invalid instruction
    if (!validInst) {
        fprintf(stderr, "%03X - Aborting - Invalid opcode %02X%02X\n",
                self->PC, op12, op34);
        exit(EXIT_FAILURE);
    }

    // Inc PC if neccessary
    if (normInc) { self->PC += 2; }
}

/*** Font data to be copied into the low ram addresses ***/
static uint8_t font5[80] = {
    /* 0 */ 0b11110000, 0b10010000, 0b10010000, 0b10010000, 0b11110000,
    /* 1 */ 0b00100000, 0b01100000, 0b00100000, 0b00100000, 0b01110000,
    /* 2 */ 0b11110000, 0b00010000, 0b11110000, 0b10000000, 0b11110000,
    /* 3 */ 0b11110000, 0b00010000, 0b11110000, 0b00010000, 0b11110000,
    /* 4 */ 0b10010000, 0b10010000, 0b11110000, 0b00010000, 0b00010000,
    /* 5 */ 0b11110000, 0b10000000, 0b11110000, 0b00010000, 0b11110000,
    /* 6 */ 0b11110000, 0b10000000, 0b11110000, 0b10010000, 0b11110000,
    /* 7 */ 0b11110000, 0b00010000, 0b00100000, 0b01000000, 0b01000000,
    /* 8 */ 0b11110000, 0b10010000, 0b11110000, 0b10010000, 0b11110000,
    /* 9 */ 0b11110000, 0b10010000, 0b11110000, 0b00010000, 0b11110000,
    /* A */ 0b11110000, 0b10010000, 0b11110000, 0b10010000, 0b10010000,
    /* B */ 0b11100000, 0b10010000, 0b11100000, 0b10010000, 0b11100000,
    /* C */ 0b11110000, 0b10000000, 0b10000000, 0b10000000, 0b11110000,
    /* D */ 0b11100000, 0b10010000, 0b10010000, 0b10010000, 0b11100000,
    /* E */ 0b11110000, 0b10000000, 0b11110000, 0b10000000, 0b11110000,
    /* F */ 0b11110000, 0b10000000, 0b11110000, 0b10000000, 0b10000000
};
static uint8_t font10[100] = {
    /* 0 */ 0b00111100, 0b01111110, 0b11100111, 0b11000011, 0b11000011,
    0b11000011, 0b11000011, 0b11100111, 0b01111110, 0b00111100,
    /* 1 */ 0b00011000, 0b00111000, 0b01011000, 0b00011000, 0b00011000,
    0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100,
    /* 2 */ 0b00111110, 0b01111111, 0b11000011, 0b00000110, 0b00001100,
    0b00011000, 0b00110000, 0b01100000, 0b11111111, 0b11111111,
    /* 3 */ 0b00111100, 0b01111110, 0b11000011, 0b00000011, 0b00001110,
    0b00001110, 0b00000011, 0b11000011, 0b01111110, 0b00111100,
    /* 4 */ 0b00000110, 0b00001110, 0b00011110, 0b00110110, 0b01100110,
    0b11000110, 0b11111111, 0b11111111, 0b00000110, 0b00000110,
    /* 5 */ 0b11111111, 0b11111111, 0b11000000, 0b11000000, 0b11111100,
    0b11111110, 0b00000011, 0b11000011, 0b01111110, 0b00111100,
    /* 6 */ 0b00111110, 0b01111100, 0b11000000, 0b11000000, 0b11111100,
    0b11111110, 0b11000011, 0b11000011, 0b01111110, 0b00111100,
    /* 7 */ 0b11111111, 0b11111111, 0b00000011, 0b00000110, 0b00001100,
    0b00011000, 0b00110000, 0b01100000, 0b01100000, 0b01100000,
    /* 8 */ 0b00111100, 0b01111110, 0b11000011, 0b11000011, 0b01111110,
    0b01111110, 0b11000011, 0b11000011, 0b01111110, 0b00111100,
    /* 9 */ 0b00111100, 0b01111110, 0b11000011, 0b11000011, 0b01111111,
    0b00111111, 0b00000011, 0b00000011, 0b00111110, 0b01111100
};
