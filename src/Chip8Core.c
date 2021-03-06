#include <stdio.h>    
#include <string.h>
#include <time.h>

#include "Chip8Core.h"
#include "Guards.h"

#define FONT_5_START  0x000
#define FONT_10_START 0x080
#define PROG_START    0x200

static bool Chip8_drawByte_LS(Chip8Proc *self, int row, int col, uint8_t data);
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
    memset(proc.ram, 0, 4096);
    memcpy(proc.ram + FONT_5_START, font5, sizeof(font5));
    memcpy(proc.ram + FONT_10_START, font10, sizeof(font10));
    memcpy(proc.ram + PROG_START, program, progSize);
    proc.PC = 0x200;
    // Init the screen buffer
    memset(proc.screen, 0, 64 * 128);
    // Seed random number generator
    srand((unsigned int) time(NULL));
    return proc;
}

bool Chip8_advance(Chip8Proc *self) {

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
                if (!self->largeScreen && op4 % 2 != 0) {
                    fprintf(stderr, "%03X - Aborting - Attempted to scroll "
                            "by an odd number of pixels in lores mode\n",
                            self->PC);
                    exit(EXIT_FAILURE);
                }
                // Move data
                memmove(self->screen + op4 * 128,
                        self->screen,
                        128 * (64 - op4));
                // Clear moved space
                memset(self->screen, 0, op4 * 128);
                // Send new framebuffer
                self->sendScreen(self->screen);
                break;
            }
            switch (op34) {
                case 0xE0: // 00E0: Clear the screen
                    validInst = true;
                    for (int c = 0; c < 128; ++c) {
                        for (int r = 0; r < 64; ++r) {
                            self->screen[r * 128 + c] = false;
                        }
                    }
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
                    // Scroll each line
                    for (int r = 0; r < 64; ++r) {
                        memmove(self->screen + 128 * r + 4,
                                self->screen + 128 * r,
                                124);
                        memset(self->screen + 128 * r, 0, 4);
                    }
                    // Send new framebuffer
                    self->sendScreen(self->screen);
                    break;
                case 0xFC: // 00FC: Scroll 4 small pixels left
                    validInst = true;
                    // Scroll each line
                    for (int r = 0; r < 64; ++r) {
                        memmove(self->screen + 128 * r,
                                self->screen + 128 * r + 4,
                                124);
                        memset(self->screen + 128 * r + 124, 0, 4);
                    }
                    // Send new framebuffer
                    self->sendScreen(self->screen);
                    break;
                case 0xFD: // 00FD: Exit interpreter
                    return false;
                case 0xFE: // 00FE: Switch to lores
                    validInst = true;
                    self->largeScreen = false;
                    break;
                case 0xFF: // 00FF: Switch to hires
                    validInst = true;
                    self->largeScreen = true;
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
                    int old = self->V[op2];
                    self->V[op2] -= self->V[op3];
                    self->V[0xF] = old >= self->V[op2];
                    break;
                case 0x6: // 8xy6: Set Vx to (superMode ? Vx : Vy) >> 1, Vf to lost bit
                    validInst = true;
                    if (self->superMode) {
                        self->V[0xF] = self->V[op2] & 1;
                        self->V[op2] >>= 1;
                    } else {
                        self->V[0xF] = self->V[op3] & 1;
                        self->V[op2] = self->V[op3] >> 1;
                    }
                    break;
                case 0x7: // 8xy7: Set Vx to Vy - Vx, Vf to !borrow
                    validInst = true;
                    int prev = self->V[op3];
                    self->V[op2] = self->V[op3] - self->V[op2];
                    self->V[0xF] = prev >= self->V[op2];
                    break;
                case 0xE: // 8xyE: Set Vx to (superMode ? Vx : Vy) << 1, Vf to lost bit
                    validInst = true;
                    if (self->superMode) {
                        self->V[0xF] = self->V[op2] & 0x80;
                        self->V[op2] <<= 1;
                    } else {
                        self->V[0xF] = self->V[op3] & 0x80;
                        self->V[op2] = self->V[op3] << 1;
                    }
                    break;
            }
            break;
        case 0x9:
            if (op4 == 0x0) { // 9xy0: Skip next instruction if Vx != Vy
                validInst = true;
                if (self->V[op2] != self->V[op3]) {
                    self->PC += 2;
                }
            }
            break;
        case 0xA: // Annn: Set I to nnn
            validInst = true;
            self->I = op2 << 8 | op34;
            break;
        case 0xB: // Bnnn: Jump to nnn + V0 (Super has a quirk, not implemented)
            validInst = true;
            self->PC = (op2 << 8 | op34) + self->V[0x0];
            normInc = false;
            break;
        case 0xC: // Cxnn: Set Vx to rand & nn
            validInst = true;
            self->V[op2] = rand() & op34;
            break;
        case 0xD:
            ; // Empty expression required cuz labels are stupid in C
            int x0 = self->V[op2], y0 = self->V[op3];
            if (op4 == 0x0) { // Dxy0: 16x16 draw in largeScreen, draw nothing otherwise
                // Does NOT replicate Super collision line count
                // Vf is set to 1 if there is a collision, 0 otherwise
                self->V[0xF] = false;
                validInst = true;
                if (self->largeScreen) {
                    for (int i = 0; i < 16; ++i) {
                        if (Chip8_drawByte_LS(self, y0 + i, x0,
                                    self->ram[self->I + 2 * i])
                                || Chip8_drawByte_LS(self, y0 + i, x0 + 8,
                                    self->ram[self->I + 2 * i + 1])) {
                            self->V[0xF] = true;
                        }
                    }
                } else {
                    break;
                }
            } else { // Dxyn: Draw n-row sprite from ram(I) at (Vx, Vy)
                // Vf is set to 1 if there is a collision, 0 otherwise
                self->V[0xF] = false;
                validInst = true;
                for (uint8_t r = 0; r < op4; ++r) {
                    uint8_t rowVal = self->ram[self->I + r];
                    if (self->largeScreen) {
                        Chip8_drawByte_LS(self, r + y0, x0, rowVal);
                    } else {
                        uint8_t mask = 0x80;
                        for (uint8_t c = 0; c < 8; ++c, mask >>= 1) {
                            if (rowVal & mask) {
                                int row = 2 * (r + y0) % 64,
                                    col = 2 * (c + x0) % 128;
                                bool curState = self->screen[row * 128 + col];
                                if (curState) {
                                    // If pixel is already on, set flag for collision
                                    self->V[0xF] = true;
                                }
                                self->screen[row * 128 + col] = !curState;
                                self->screen[(row + 1) * 128 + col] = !curState;
                                self->screen[row * 128 + col + 1] = !curState;
                                self->screen[(row + 1) * 128 + col + 1] = !curState;
                            }
                        }
                    }
                }
            }
            // Send new framebuffer
            self->sendScreen(self->screen);
            break;
        case 0xE:
            switch (op34) {
                case 0x9E: // Ex9E: Skip next instruction if key Vx is not pressed
                    // validInst = true;
                    // TODO Ex9E
                    break;
                case 0xA1: // ExA1: Skip next instruction if key Vx is pressed
                    // validInst = true;
                    // TODO ExA1
                    break;
            }
            break;
        case 0xF:
            switch (op34) {
                case 0x07: // Fx07: Set Vx to D
                    validInst = true;
                    self->V[op2] = self->D;
                    break;
                case 0x0A: // Fx0A: Wait for keypress, then set Vx to key
                    // validInst = true;
                    // TODO Fx0A
                    break;
                case 0x15: // Fx15: Set D to Vx
                    validInst = true;
                    self->D = self->V[op2];
                    break;
                case 0x18: // Fx18: Set S to Vx, call setSound(true, self)
                    // validInst = true;
                    // TODO Fx18
                    break;
                case 0x1E: // Fx1E: Set I to I + Vx, Super has an (ignored) quirk
                    validInst = true;
                    self->I += self->V[op2];
                    break;
                case 0x29: // Fx29: Point I to 5-wide sprite for the hex char in Vx
                    validInst = true;
                    self->I = FONT_5_START + 5 * (self->V[op2] % 16);
                    break;
                case 0x30: // Fx30: Point I to 10-wide sprite for the num char in Vx
                    validInst = true;
                    if (self->V[op2] % 16 > 0x9) {
                        fprintf(stderr, "%03X - Aborting - Large sprites are only "
                                "available for characters 0-9\n", self->PC);
                        exit(EXIT_FAILURE);
                    }
                    self->I = FONT_10_START + 10 * (self->V[op2] % 16);
                    break;
                case 0x33: // Fx33: Set I, I+1, I+2 to the decimal digits of Vx
                    validInst = true;
                    int val = self->V[op2];
                    self->ram[self->I] = val / 100;
                    val %= 100;
                    self->ram[self->I + 1] = val / 10;
                    val %= 10;
                    self->ram[self->I + 2] = val;
                    break;
                case 0x55: // Fx55: Store V0...Vx to ram starting at I
                    // When not in superMode, I will be incremented
                    // In superMode, I stays constant
                    validInst = true;
                    for (int i = 0; i < op2; ++i) {
                        self->ram[self->I + i] = self->V[i];
                    }
                    if (!self->superMode) { self->I += op2; }
                    break;
                case 0x65: // Fx65: Read V0...Vx from ram starting at I
                    // When not in superMode, I will be incremented
                    // In superMode, I stays constant
                    validInst = true;
                    for (int i = 0; i < op2; ++i) {
                        self->V[i] = self->ram[self->I + i];
                    }
                    if (!self->superMode) { self->I += op2; }
                    break;
                case 0x75: // Fx75: Store V0...Vx to flag registers (x < 8, Super only)
                    validInst = true;
                    if (op2 > 7) {
                        fprintf(stderr, "%03X - Aborting - Fx75 can only store up to V7\n",
                                self->PC);
                        exit(EXIT_FAILURE);
                    }
                    for (int i = 0; i < op2; ++i) {
                        self->V[i] = self->FR[i];
                    }
                    break;
                case 0x85: // Fx85: Read V0...Vx from flag registers (x < 8, Super only)
                    validInst = true;
                    if (op2 > 7) {
                        fprintf(stderr, "%03X - Aborting - Fx85 can only read up to V7\n",
                                self->PC);
                        exit(EXIT_FAILURE);
                    }
                    for (int i = 0; i < op2; ++i) {
                        self->V[i] = self->FR[i];
                    }
                    break;
            }
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

    // Return true to indicate processor is still running
    return true;
}

static bool Chip8_drawByte_LS(Chip8Proc *self, int row, int col, uint8_t data) {
    bool anyInverts = false;
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1, col++) {
        if (data & mask) {
            bool *pixel = &self->screen[128 * (row % 64) + (col % 128)];
            *pixel = !*pixel;
            if (!*pixel) { anyInverts = true; }
        }
    }
    return anyInverts;
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
