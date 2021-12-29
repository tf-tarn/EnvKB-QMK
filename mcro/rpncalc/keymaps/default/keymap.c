/* Copyright 2021 sekigon-gonnoc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include QMK_KEYBOARD_H
#include "pico_eeprom.h"

enum rpn_state {
    S_NUMBER,
};

enum custom_keycodes {
    RPN_0 = SAFE_RANGE,
    RPN_1,
    RPN_2,
    RPN_3,
    RPN_4,
    RPN_5,
    RPN_6,
    RPN_7,
    RPN_8,
    RPN_9,
    RPN_ENTER,
    RPN_BSPC,
    RPN_ADD,
    RPN_SUB,
    RPN_NEG,
    RPN_DIV,
    RPN_MUL,
    RPN_DUP,
    RPN_POP,
    RPN_PEEK,
    RPN_SWAP,
    RPN_LIST
};

#define STACKSIZE 1024
static int32_t stack[1024];
static uint16_t stackpos = 0;
#define MAX_INPUT 15
static uint8_t inbuf[MAX_INPUT + 1];
static uint8_t inbufpos = 0;
static int state = S_NUMBER;

void record_input(char digit) {
    if (inbufpos >= MAX_INPUT) {
        return;
    }
    inbuf[inbufpos++] = digit;
}

void backspace_input() {
    if (inbufpos <= 0) {
        return;
    }
    --inbufpos;
}

int32_t pop_inbuf() {
    int32_t magnitude = 1;
    int32_t value = 0;
    for (int i = inbufpos - 1; i >= 0; --i) {
        value += inbuf[i] * magnitude;
        magnitude *= 10;
        inbuf[i] = 0;
    }
    inbufpos = 0;
    return value;
}

void stack_push(int32_t value) {
    if (stackpos >= STACKSIZE) {
        return;
    }
    stack[stackpos++] = value;
}

int32_t stack_pop() {
    if (stackpos < 1) {
        return 0;
    }
    return stack[--stackpos];
}

int32_t stack_peek() {
    return stack[stackpos - 1];
}

static uint16_t keyvalmap[10] = {
    KC_0,
    KC_1,
    KC_2,
    KC_3,
    KC_4,
    KC_5,
    KC_6,
    KC_7,
    KC_8,
    KC_9
};

void send_int(int32_t value) {
    static uint16_t outbuf[12];
    for (int i = 0; i < 12; ++i) {
        outbuf[i] = 0;
    }
    for (int i = 0; i < 12; ++i) {
        outbuf[i] = value % 10;
        value /= 10;
    }
    bool start = true;
    for (int i = 11; i >= 0; --i) {
        if (start && i > 0) {
            if (outbuf[i] == 0) {
                continue;
            } else {
                start = false;
            }
        }
        tap_code(keyvalmap[outbuf[i]]);
    }
}

void input_push() {
    if (inbufpos > 0) {
        int32_t v = pop_inbuf();
        stack_push(v);
        SEND_STRING("<\n");
    }
}

static char dirty_hack[2] = {0, 0};

bool process_record_user(uint16_t kc, keyrecord_t *record) {
#define DO_NUMBER(n) case RPN_##n: dirty_hack[0] = '0' + n; send_string(dirty_hack); record_input(n); break;
    if (record->event.pressed) {

        switch (state) {
        case S_NUMBER:
            switch (kc) {
                // case RPN_0: SS_TAP(X_0); record_input(0); break;
                DO_NUMBER(0);
                DO_NUMBER(1);
                DO_NUMBER(2);
                DO_NUMBER(3);
                DO_NUMBER(4);
                DO_NUMBER(5);
                DO_NUMBER(6);
                DO_NUMBER(7);
                DO_NUMBER(8);
                DO_NUMBER(9);
            case RPN_BSPC:
                backspace_input();
                tap_code(KC_BSPC);
                break;
            case RPN_ENTER:
                input_push();
                break;
            case RPN_POP:
            {
                input_push();
                if (stackpos > 0) {
                    int32_t v = stack_pop();
                    SEND_STRING("> ");
                    send_int(v);
                    SEND_STRING("\n");
                } else {
                    SEND_STRING("# empty\n");
                }
                break;
            }
            case RPN_PEEK:
            {
                input_push();
                if (stackpos > 0) {
                    SEND_STRING("# ");
                    send_int(stack_peek());
                    SEND_STRING("\n");
                } else {
                    SEND_STRING("# empty\n");
                }
                break;
            }
            case RPN_ADD:
                input_push();
                if (stackpos > 1) {
                    stack_push(stack_pop() + stack_pop());
                    SEND_STRING("+\n");
                } else {
                    SEND_STRING("# can't +\n");
                }
                break;

            case RPN_SUB:
                input_push();
                if (stackpos > 1) {
                    int32_t v = stack_pop();
                    stack_push(stack_pop() - v);
                    SEND_STRING("-\n");
                } else {
                    SEND_STRING("# can't -\n");
                }
                break;

            case RPN_NEG:
                input_push();
                if (stackpos > 0) {
                    stack_push(-stack_pop());
                    SEND_STRING("-.\n");
                } else {
                    SEND_STRING("# can't negate\n");
                }
                break;

            case RPN_MUL:
                input_push();
                if (stackpos > 1) {
                    stack_push(stack_pop() * stack_pop());
                    SEND_STRING("*\n");
                } else {
                    SEND_STRING("# can't *\n");
                }
                break;

            case RPN_DIV:
                input_push();
                if (stackpos > 1) {
                    int32_t v = stack_pop();
                    stack_push(stack_pop() / v);
                    SEND_STRING("/\n");
                } else {
                    SEND_STRING("# can't /\n");
                }
                break;

            case RPN_DUP:
                input_push();
                if (stackpos > 0) {
                    int32_t v = stack_pop();
                    stack_push(v);
                    stack_push(v);
                    SEND_STRING("dup\n");
                } else {
                    SEND_STRING("# can't dup\n");
                }
                break;

            case RPN_SWAP:
                input_push();
                if (stackpos > 1) {
                    int32_t v1 = stack_pop();
                    int32_t v2 = stack_pop();
                    stack_push(v1);
                    stack_push(v2);
                    SEND_STRING("swap\n");
                } else {
                    SEND_STRING("# can't dup\n");
                }
                break;

            case RPN_LIST:
                input_push();
                SEND_STRING("#   begin stack dump\n");
                for (int i = stackpos - 1; i >= 0; --i) {
                    SEND_STRING("# ");
                    send_int(stack[i]);
                    SEND_STRING("\n");
                }
                SEND_STRING("#   end stack dump\n");
                break;

            }
        }

    } else {
    }
    return true;
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Base */
    [0] = {
        { OSL(1),    RPN_7, RPN_4, RPN_1 },
        { RPN_0,     RPN_8, RPN_5, RPN_2 },
        { RPN_ENTER, RPN_9, RPN_6, RPN_3 },
    },
    [1] = {
        { KC_TRNS,   RPN_DUP,  RPN_MUL,  RPN_ADD },
        { RPN_LIST,  RPN_PEEK, RPN_DIV,  RPN_SUB },
        { RPN_SWAP,  RPN_POP,  RPN_BSPC,  RPN_NEG },
    },
};


