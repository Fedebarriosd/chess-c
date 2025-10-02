#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>

// Bitboards globales
extern uint64_t WP, WN, WB, WR, WQ, WK;
extern uint64_t BP, BN, BB, BR, BQ, BK;

// Utilidades básicas
int      square_index(int file, int rank);
uint64_t bit_at(int sq);

// Ocupación
uint64_t occ_white(void);
uint64_t occ_black(void);
uint64_t occ_all(void);

// Consultas
int piece_code_at(int sq);  // 0..11 o -1 si vacío
int is_white_at(int sq);    // 1 si blanca
int is_black_at(int sq);    // 1 si negra

// ----- En passant (estado) -----
int  get_ep_square(void);   // -1 si no hay EP
void set_ep_square(int sq);
void clear_ep_square(void);

// ----- Derechos de enroque -----
int  get_castle_rights(void);     // bitmask: 1=WK,2=WQ,4=BK,8=BQ
void set_castle_rights(int rights);
void clear_castle_rights(void);

// ----- Ataques precomputados / init -----
void board_init_attacks(void);   // init tablas (caballo, rey)

// ¿Está atacada la casilla 'sq' por 'side' (1=blancas, 0=negras)?
int is_square_attacked_by_side(int sq, int side);

// ¿Rey de 'side' en jaque?
int is_king_in_check(int side);

// ----- Generación de movimientos -----
// Pseudolegales (incluye rey + enroques con chequeos básicos)
uint64_t gen_moves_from(int sq, int sideToMove);

// Legales = filtra pseudolegales que dejan al propio rey en jaque
uint64_t gen_legal_moves_from(int sq, int sideToMove);

// ----- Hacer movimiento (con promoción + EP + enroque) -----
// promoteCode: -1 = auto-dama.
// Blancas: 1=N,2=B,3=R,4=Q  |  Negras: 7=N,8=B,9=R,10=Q
int move_make(int fromSq, int toSq, int sideToMove, int promoteCode);

// ----- Perft (legal) -----
uint64_t perft(int depth, int sideToMove);
void perft_divide(int depth, int sideToMove);

// Inicialización
void board_init_startpos(void);

#endif // BOARD_H
