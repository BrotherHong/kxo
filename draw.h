#ifndef DRAW_H
#define DRAW_H
#define DRAW_SPACE 0b00
#define DRAW_X 0b01
#define DRAW_O 0b10
#define DRAW_GET_BIT(symb) \
    ((symb) == ' ' ? DRAW_SPACE : ((symb == 'X') ? DRAW_X : DRAW_O))
/*
 * DRAW_XO(chr, symb, i) sets the i-th bit of chr to the value of symb
 * chr: the character to be modified
 * symb: the symbol to be set (X or O or space)
 * i: the index of the bit to be set (0-3)
 */
#define DRAW_XO(chr, symb, i) ((chr) |= (DRAW_GET_BIT(symb) << ((i) << 1)))
#define DRAW_XO_GET_BIT(chr, i) (((chr) & ((0b11 << ((i) << 1)))) >> ((i) << 1))
#define DRAW_XO_GET_SYMB(chr, i)             \
    ((DRAW_XO_GET_BIT(chr, i) == DRAW_SPACE) \
         ? ' '                               \
         : ((DRAW_XO_GET_BIT(chr, i) == DRAW_X) ? 'X' : 'O'))
#endif