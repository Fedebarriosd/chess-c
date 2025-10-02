#include "board.h"
#include <stdio.h>

/* ---------------- Bitboards de piezas ---------------- */
uint64_t WP, WN, WB, WR, WQ, WK;
uint64_t BP, BN, BB, BR, BQ, BK;

int square_index(int file, int rank) { return rank * 8 + file; }
uint64_t bit_at(int sq) { return 1ULL << sq; }

uint64_t occ_white(void) { return WP|WN|WB|WR|WQ|WK; }
uint64_t occ_black(void) { return BP|BN|BB|BR|BQ|BK; }
uint64_t occ_all(void)   { return occ_white() | occ_black(); }

static uint64_t* piece_bb[12] = {
    &WP,&WN,&WB,&WR,&WQ,&WK,
    &BP,&BN,&BB,&BR,&BQ,&BK
};

/* ---------------- Consultas ---------------- */
int piece_code_at(int sq){
    uint64_t m = bit_at(sq);
    if (WP&m) return 0; if (WN&m) return 1; if (WB&m) return 2;
    if (WR&m) return 3; if (WQ&m) return 4; if (WK&m) return 5;
    if (BP&m) return 6; if (BN&m) return 7; if (BB&m) return 8;
    if (BR&m) return 9; if (BQ&m) return 10; if (BK&m) return 11;
    return -1;
}
int is_white_at(int sq){ int c = piece_code_at(sq); return (c >= 0 && c <= 5); }
int is_black_at(int sq){ int c = piece_code_at(sq); return (c >= 6 && c <= 11); }

/* ---------------- En Passant (estado) ---------------- */
static int gEpSquare = -1;
int  get_ep_square(void) { return gEpSquare; }
void set_ep_square(int sq) { gEpSquare = sq; }
void clear_ep_square(void) { gEpSquare = -1; }

/* ---------------- Derechos de enroque ---------------- */
static int gCastleRights = 0; // bitmask: 1=WK,2=WQ,4=BK,8=BQ
int  get_castle_rights(void)     { return gCastleRights; }
void set_castle_rights(int r)    { gCastleRights = r; }
void clear_castle_rights(void)   { gCastleRights = 0; }

/* ---------------- Máscaras útiles ---------------- */
static const uint64_t FILE_A = 0x0101010101010101ULL;
static const uint64_t FILE_B = 0x0202020202020202ULL;
static const uint64_t FILE_G = 0x4040404040404040ULL;
static const uint64_t FILE_H = 0x8080808080808080ULL;

static const uint64_t NOT_FILE_A = 0xfefefefefefefefeULL;
static const uint64_t NOT_FILE_B = 0xfdfdfdfdfdfdfdfdULL;
static const uint64_t NOT_FILE_G = 0xbfbfbfbfbfbfbfbfULL;
static const uint64_t NOT_FILE_H = 0x7f7f7f7f7f7f7f7fULL;

static const uint64_t RANK_2 = 0x000000000000FF00ULL;
static const uint64_t RANK_7 = 0x00FF000000000000ULL;

/* ---------------- Generación: Peones ---------------- */
static uint64_t gen_pawn_from(int sq, int sideToMove) {
    uint64_t m = bit_at(sq);
    uint64_t empty = ~occ_all();
    uint64_t moves = 0ULL;

    if (sideToMove == 1) { // blancas
        uint64_t one = (m << 8) & empty;
        moves |= one;
        if ((m & RANK_2) && one) {
            uint64_t two = (m << 16) & empty & (empty << 8);
            moves |= two;
        }
        uint64_t capL = ((m & ~FILE_A) << 7) & occ_black();
        uint64_t capR = ((m & ~FILE_H) << 9) & occ_black();
        moves |= capL | capR;

        if (gEpSquare != -1) {
            uint64_t epMask = bit_at(gEpSquare);
            uint64_t epL = ((m & ~FILE_A) << 7) & epMask;
            uint64_t epR = ((m & ~FILE_H) << 9) & epMask;
            moves |= epL | epR;
        }
    } else { // negras
        uint64_t one = (m >> 8) & empty;
        moves |= one;
        if ((m & RANK_7) && one) {
            uint64_t two = (m >> 16) & empty & (empty >> 8);
            moves |= two;
        }
        uint64_t capL = ((m & ~FILE_H) >> 7) & occ_white();
        uint64_t capR = ((m & ~FILE_A) >> 9) & occ_white();
        moves |= capL | capR;

        if (gEpSquare != -1) {
            uint64_t epMask = bit_at(gEpSquare);
            uint64_t epL = ((m & ~FILE_H) >> 7) & epMask;
            uint64_t epR = ((m & ~FILE_A) >> 9) & epMask;
            moves |= epL | epR;
        }
    }
    return moves;
}

/* ---------------- Ataques precomputados: Caballos y Rey ---------------- */
static uint64_t KNIGHT_ATTACKS[64];
static uint64_t KING_ATTACKS[64];

static inline uint64_t shift_north(uint64_t b, int n){ return b << (8*n); }
static inline uint64_t shift_south(uint64_t b, int n){ return b >> (8*n); }

void board_init_attacks(void) {
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t m = bit_at(sq);
        // Caballo
        uint64_t atkN = 0ULL;
        atkN |= shift_north((m & NOT_FILE_H), 2) << 1;
        atkN |= shift_north((m & NOT_FILE_A), 2) >> 1;
        atkN |= shift_south((m & NOT_FILE_H), 2) << 1;
        atkN |= shift_south((m & NOT_FILE_A), 2) >> 1;
        atkN |= ((m & NOT_FILE_G & NOT_FILE_H) << 2) << 8;
        atkN |= ((m & NOT_FILE_G & NOT_FILE_H) << 2) >> 8;
        atkN |= ((m & NOT_FILE_A & NOT_FILE_B) >> 2) << 8;
        atkN |= ((m & NOT_FILE_A & NOT_FILE_B) >> 2) >> 8;
        KNIGHT_ATTACKS[sq] = atkN;

        // Rey (8 adyacentes con bordes)
        uint64_t atkK = 0ULL;
        uint64_t notA = NOT_FILE_A, notH = NOT_FILE_H;
        atkK |= (m & notH) << 1;                // E
        atkK |= (m & notA) >> 1;                // W
        atkK |= m << 8;                         // N
        atkK |= m >> 8;                         // S
        atkK |= (m & notH) << 9;                // NE
        atkK |= (m & notA) << 7;                // NW
        atkK |= (m & notH) >> 7;                // SE
        atkK |= (m & notA) >> 9;                // SW
        KING_ATTACKS[sq] = atkK;
    }
}

/* ---------------- Ataques: Alfiles (raycast en 4 diagonales) ---------------- */
static uint64_t bishop_attacks_on_the_fly(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;

    for (int ff=f+1, rr=r+1; ff<8 && rr<8; ++ff, ++rr) { int s=rr*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int ff=f-1, rr=r+1; ff>=0 && rr<8; --ff, ++rr) { int s=rr*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int ff=f+1, rr=r-1; ff<8 && rr>=0; ++ff, --rr) { int s=rr*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int ff=f-1, rr=r-1; ff>=0 && rr>=0; --ff, --rr) { int s=rr*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    return attacks;
}

/* ---------------- Ataques: Torres (raycast ortogonal) ---------------- */
static uint64_t rook_attacks_on_the_fly(int sq, uint64_t occ) {
    uint64_t attacks = 0ULL;
    int f = sq % 8, r = sq / 8;

    for (int rr=r+1; rr<8; ++rr) { int s=rr*8+f; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int rr=r-1; rr>=0; --rr){ int s=rr*8+f; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int ff=f+1; ff<8; ++ff) { int s=r*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    for (int ff=f-1; ff>=0; --ff){ int s=r*8+ff; uint64_t bm=bit_at(s); attacks|=bm; if (occ&bm) break; }
    return attacks;
}

/* ---------------- ¿Casilla atacada por side? ---------------- */
int is_square_attacked_by_side(int sq, int side) {
    uint64_t target = bit_at(sq);
    uint64_t occ = occ_all();

    if (side == 1) { // blancas atacan
        // peones blancos
        uint64_t pawnAtk = ((WP & NOT_FILE_A) << 7) | ((WP & NOT_FILE_H) << 9);
        if (pawnAtk & target) return 1;
        // caballos
        uint64_t nBB = WN; while (nBB){ int s=__builtin_ctzll(nBB); nBB&=nBB-1; if (KNIGHT_ATTACKS[s] & target) return 1; }
        // alfiles/damas (diagonales)
        uint64_t bBB = WB|WQ; while (bBB){ int s=__builtin_ctzll(bBB); bBB&=bBB-1; if (bishop_attacks_on_the_fly(s, occ) & target) return 1; }
        // torres/damas (ortogonales)
        uint64_t rBB = WR|WQ; while (rBB){ int s=__builtin_ctzll(rBB); rBB&=rBB-1; if (rook_attacks_on_the_fly(s, occ) & target) return 1; }
        // rey
        if (WK) { int ks = __builtin_ctzll(WK); if (KING_ATTACKS[ks] & target) return 1; }
    } else { // negras atacan
        uint64_t pawnAtk = ((BP & NOT_FILE_H) >> 7) | ((BP & NOT_FILE_A) >> 9);
        if (pawnAtk & target) return 1;
        uint64_t nBB = BN; while (nBB){ int s=__builtin_ctzll(nBB); nBB&=nBB-1; if (KNIGHT_ATTACKS[s] & target) return 1; }
        uint64_t bBB = BB|BQ; while (bBB){ int s=__builtin_ctzll(bBB); bBB&=bBB-1; if (bishop_attacks_on_the_fly(s, occ) & target) return 1; }
        uint64_t rBB = BR|BQ; while (rBB){ int s=__builtin_ctzll(rBB); rBB&=rBB-1; if (rook_attacks_on_the_fly(s, occ) & target) return 1; }
        if (BK) { int ks = __builtin_ctzll(BK); if (KING_ATTACKS[ks] & target) return 1; }
    }
    return 0;
}

/* -------------- Rey en jaque -------------- */
static int king_square(int side){
    if (side==1) { if (!WK) return -1; return __builtin_ctzll(WK); }
    else         { if (!BK) return -1; return __builtin_ctzll(BK); }
}
int is_king_in_check(int side){
    int ks = king_square(side);
    if (ks < 0) return 0; // sin rey (pos irregular)
    return is_square_attacked_by_side(ks, 1-side);
}

/* ---------------- Dispatcher: gen_moves_from (pseudolegal) ---------------- */
uint64_t gen_moves_from(int sq, int sideToMove) {
    int code = piece_code_at(sq);
    if (code == -1) return 0ULL;
    if ((sideToMove==1 && !(code <= 5)) || (sideToMove==0 && !(code >= 6))) return 0ULL;

    // Peones
    if (code == 0 || code == 6) return gen_pawn_from(sq, sideToMove);

    // Caballos
    if (code == 1 || code == 7) {
        uint64_t own = (sideToMove==1) ? occ_white() : occ_black();
        return KNIGHT_ATTACKS[sq] & ~own;
    }

    // Alfiles
    if (code == 2 || code == 8) {
        uint64_t occ = occ_all();
        uint64_t own = (sideToMove==1) ? occ_white() : occ_black();
        uint64_t atk = bishop_attacks_on_the_fly(sq, occ);
        return atk & ~own;
    }

    // Torres
    if (code == 3 || code == 9) {
        uint64_t occ = occ_all();
        uint64_t own = (sideToMove==1) ? occ_white() : occ_black();
        uint64_t atk = rook_attacks_on_the_fly(sq, occ);
        return atk & ~own;
    }

    // Dama
    if (code == 4 || code == 10) {
        uint64_t occ = occ_all();
        uint64_t own = (sideToMove==1) ? occ_white() : occ_black();
        uint64_t atkB = bishop_attacks_on_the_fly(sq, occ);
        uint64_t atkR = rook_attacks_on_the_fly(sq, occ);
        return (atkB | atkR) & ~own;
    }

    // Rey (+ enroques con chequeo de casillas atacadas)
    if (code == 5 || code == 11) {
        uint64_t own = (sideToMove==1) ? occ_white() : occ_black();
        uint64_t opp = 1 - sideToMove;
        uint64_t moves = KING_ATTACKS[sq] & ~own;

        // filtrar casillas atacadas (rey no puede entrar en jaque)
        uint64_t safe = 0ULL;
        uint64_t tmp = moves;
        while (tmp) {
            int tsq = __builtin_ctzll(tmp); tmp &= tmp - 1;
            if (!is_square_attacked_by_side(tsq, opp)) safe |= bit_at(tsq);
        }
        moves = safe;

        // --- Enroques ---
        int rights = get_castle_rights();
        uint64_t all = occ_all();

        if (sideToMove == 1 && sq == square_index(4,0)) { // e1 blanco
            if ((rights & 1) && !(all & (bit_at(5)|bit_at(6))) &&
                !is_square_attacked_by_side(4, opp) &&
                !is_square_attacked_by_side(5, opp) &&
                !is_square_attacked_by_side(6, opp)) {
                moves |= bit_at(6);
            }
            if ((rights & 2) && !(all & (bit_at(3)|bit_at(2)|bit_at(1))) &&
                !is_square_attacked_by_side(4, opp) &&
                !is_square_attacked_by_side(3, opp) &&
                !is_square_attacked_by_side(2, opp)) {
                moves |= bit_at(2);
            }
        } else if (sideToMove == 0 && sq == square_index(4,7)) { // e8 negro
            if ((rights & 4) && !(all & (bit_at(61)|bit_at(62))) &&
                !is_square_attacked_by_side(60, 1) &&
                !is_square_attacked_by_side(61, 1) &&
                !is_square_attacked_by_side(62, 1)) {
                moves |= bit_at(62);
            }
            if ((rights & 8) && !(all & (bit_at(59)|bit_at(58)|bit_at(57))) &&
                !is_square_attacked_by_side(60, 1) &&
                !is_square_attacked_by_side(59, 1) &&
                !is_square_attacked_by_side(58, 1)) {
                moves |= bit_at(58);
            }
        }
        return moves;
    }

    return 0ULL;
}

/* ---------------- Legales: filtrar pseudolegales ---------------- */
uint64_t gen_legal_moves_from(int sq, int sideToMove){
    uint64_t legal = 0ULL;
    uint64_t pseudo = gen_moves_from(sq, sideToMove);
    while (pseudo){
        int toSq = __builtin_ctzll(pseudo);
        pseudo &= pseudo - 1;

        // snapshot
        uint64_t savedWP=WP, savedWN=WN, savedWB=WB, savedWR=WR, savedWQ=WQ, savedWK=WK;
        uint64_t savedBP=BP, savedBN=BN, savedBB=BB, savedBR=BR, savedBQ=BQ, savedBK=BK;
        int savedEP = get_ep_square();
        int savedCR = get_castle_rights();

        move_make(sq, toSq, sideToMove, -1);
        if (!is_king_in_check(sideToMove)) legal |= bit_at(toSq);

        // undo
        WP=savedWP; WN=savedWN; WB=savedWB; WR=savedWR; WQ=savedWQ; WK=savedWK;
        BP=savedBP; BN=savedBN; BB=savedBB; BR=savedBR; BQ=savedBQ; BK=savedBK;
        set_ep_square(savedEP);
        set_castle_rights(savedCR);
    }
    return legal;
}

/* ---------------- Move make con promos + EP + enroque ---------------- */
static int map_promo(int sideToMove, int promoteCode) {
    if (promoteCode >= 0) return promoteCode; // ya especificado
    return (sideToMove==1) ? 4 : 10; // default: dama
}

static void update_castle_rights_on_move(int fromSq, int toSq, int code) {
    // Si mueve un rey: pierde ambos derechos
    if (code == 5) { gCastleRights &= ~(1|2); }       // WK,WQ
    if (code == 11){ gCastleRights &= ~(4|8); }       // BK,BQ

    // Si mueve una torre desde su casilla original: pierde ese lado
    if (code == 3) { // torre blanca
        if (fromSq == square_index(0,0)) gCastleRights &= ~2; // WQ
        if (fromSq == square_index(7,0)) gCastleRights &= ~1; // WK
    }
    if (code == 9) { // torre negra
        if (fromSq == square_index(0,7)) gCastleRights &= ~8; // BQ
        if (fromSq == square_index(7,7)) gCastleRights &= ~4; // BK
    }

    // Si capturamos una torre original rival en su casilla: quita derecho rival
    if (toSq == square_index(0,0)) gCastleRights &= ~2;
    if (toSq == square_index(7,0)) gCastleRights &= ~1;
    if (toSq == square_index(0,7)) gCastleRights &= ~8;
    if (toSq == square_index(7,7)) gCastleRights &= ~4;
}

int move_make(int fromSq, int toSq, int sideToMove, int promoteCode) {
    if (fromSq<0||fromSq>63||toSq<0||toSq>63) return 0;

    int code = piece_code_at(fromSq);
    if (code == -1) return 0;
    int isWhite = (code <= 5);
    if ((sideToMove==1 && !isWhite) || (sideToMove==0 && isWhite)) return 0;

    uint64_t fromM = bit_at(fromSq), toM = bit_at(toSq);
    int isPawn = (code==0 || code==6);

    // --- Enroques (mueve el rey de e1/e8 a g/c) ---
    if (code == 5 && fromSq == square_index(4,0)) { // rey blanco
        if (toSq == square_index(6,0)) { // O-O
            WK &= ~fromM; WK |= toM;
            WR &= ~bit_at(square_index(7,0));
            WR |= bit_at(square_index(5,0));
            gCastleRights &= ~(1|2);
            clear_ep_square();
            return 1;
        }
        if (toSq == square_index(2,0)) { // O-O-O
            WK &= ~fromM; WK |= toM;
            WR &= ~bit_at(square_index(0,0));
            WR |= bit_at(square_index(3,0));
            gCastleRights &= ~(1|2);
            clear_ep_square();
            return 1;
        }
    }
    if (code == 11 && fromSq == square_index(4,7)) { // rey negro
        if (toSq == square_index(6,7)) { // O-O
            BK &= ~fromM; BK |= toM;
            BR &= ~bit_at(square_index(7,7));
            BR |= bit_at(square_index(5,7));
            gCastleRights &= ~(4|8);
            clear_ep_square();
            return 1;
        }
        if (toSq == square_index(2,7)) { // O-O-O
            BK &= ~fromM; BK |= toM;
            BR &= ~bit_at(square_index(0,7));
            BR |= bit_at(square_index(3,7));
            gCastleRights &= ~(4|8);
            clear_ep_square();
            return 1;
        }
    }

    // en passant
    if (isPawn && get_ep_square() != -1 && toSq == get_ep_square()) {
        *piece_bb[code] &= ~fromM;
        *piece_bb[code] |= toM;
        if (isWhite) { *piece_bb[6] &= ~bit_at(toSq-8); } // quita peón negro
        else         { *piece_bb[0] &= ~bit_at(toSq+8); } // quita peón blanco
        clear_ep_square();
        update_castle_rights_on_move(fromSq, toSq, code);
        return 1;
    }

    // captura normal (eliminar destino enemigo primero)
    if (isWhite) { for (int i=6;i<=11;i++) *piece_bb[i] &= ~toM; }
    else         { for (int i=0;i<=5; i++) *piece_bb[i] &= ~toM; }

    // quitar del origen
    *piece_bb[code] &= ~fromM;

    int toRank = toSq / 8;
    if (isPawn) {
        int promote = -1;
        if (isWhite && toRank==7) promote = map_promo(1, promoteCode);
        else if (!isWhite && toRank==0) promote = map_promo(0, promoteCode);

        if (promote!=-1) *piece_bb[promote] |= toM;
        else             *piece_bb[code]    |= toM;

        // EP
        int fromRank = fromSq/8;
        if (isWhite && fromRank==1 && toRank==3) set_ep_square(fromSq+8);
        else if (!isWhite && fromRank==6 && toRank==4) set_ep_square(fromSq-8);
        else clear_ep_square();
    } else {
        *piece_bb[code] |= toM;
        clear_ep_square();
    }

    update_castle_rights_on_move(fromSq, toSq, code);
    return 1;
}

/* ---------------- Posición inicial ---------------- */
void board_init_startpos(void){
    WP = WN = WB = WR = WQ = WK = 0ULL;
    BP = BN = BB = BR = BQ = BK = 0ULL;

    for (int f=0; f<8; f++) {
        WP |= bit_at(square_index(f,1));
        BP |= bit_at(square_index(f,6));
    }
    WR |= bit_at(square_index(0,0)) | bit_at(square_index(7,0));
    WN |= bit_at(square_index(1,0)) | bit_at(square_index(6,0));
    WB |= bit_at(square_index(2,0)) | bit_at(square_index(5,0));
    WQ |= bit_at(square_index(3,0));
    WK |= bit_at(square_index(4,0));

    BR |= bit_at(square_index(0,7)) | bit_at(square_index(7,7));
    BN |= bit_at(square_index(1,7)) | bit_at(square_index(6,7));
    BB |= bit_at(square_index(2,7)) | bit_at(square_index(5,7));
    BQ |= bit_at(square_index(3,7));
    BK |= bit_at(square_index(4,7));

    clear_ep_square();
    set_castle_rights(1|2|4|8); // WK|WQ|BK|BQ habilitados al inicio
    board_init_attacks();       // init caballo+rey
}

/* ---------------- Perft (legal) y divide ---------------- */

// helpers
static void sq_to_coord(int sq, char out[3]) {
    int file = sq % 8, rank = sq / 8;
    out[0] = 'a' + file; out[1] = '1' + rank; out[2] = '\0';
}
static void move_to_uci(int fromSq, int toSq, char out[6]) {
    char a[3], b[3]; sq_to_coord(fromSq, a); sq_to_coord(toSq, b);
    out[0]=a[0]; out[1]=a[1]; out[2]=b[0]; out[3]=b[1]; out[4]='\0';
}

uint64_t perft(int depth, int sideToMove) {
    if (depth == 0) return 1ULL;
    uint64_t nodes = 0ULL;

    for (int sq = 0; sq < 64; ++sq) {
        if (piece_code_at(sq) == -1) continue;
        if ((sideToMove==1 && !is_white_at(sq)) ||
            (sideToMove==0 && !is_black_at(sq))) continue;

        uint64_t moves = gen_legal_moves_from(sq, sideToMove);
        while (moves) {
            int toSq = __builtin_ctzll(moves); moves &= moves - 1;

            // snapshot
            uint64_t savedWP=WP, savedWN=WN, savedWB=WB, savedWR=WR, savedWQ=WQ, savedWK=WK;
            uint64_t savedBP=BP, savedBN=BN, savedBB=BB, savedBR=BR, savedBQ=BQ, savedBK=BK;
            int savedEP = get_ep_square();
            int savedCR = get_castle_rights();

            move_make(sq, toSq, sideToMove, -1);
            nodes += perft(depth-1, 1-sideToMove);

            // undo
            WP=savedWP; WN=savedWN; WB=savedWB; WR=savedWR; WQ=savedWQ; WK=savedWK;
            BP=savedBP; BN=savedBN; BB=savedBB; BR=savedBR; BQ=savedBQ; BK=savedBK;
            set_ep_square(savedEP);
            set_castle_rights(savedCR);
        }
    }
    return nodes;
}

void perft_divide(int depth, int sideToMove) {
    if (depth <= 0) { printf("depth debe ser >= 1\n"); return; }

    uint64_t total = 0ULL;

    for (int sq = 0; sq < 64; ++sq) {
        if (piece_code_at(sq) == -1) continue;
        if ((sideToMove==1 && !is_white_at(sq)) ||
            (sideToMove==0 && !is_black_at(sq))) continue;

        uint64_t moves = gen_legal_moves_from(sq, sideToMove);
        while (moves) {
            int toSq = __builtin_ctzll(moves); moves &= moves - 1;

            // snapshot
            uint64_t savedWP=WP, savedWN=WN, savedWB=WB, savedWR=WR, savedWQ=WQ, savedWK=WK;
            uint64_t savedBP=BP, savedBN=BN, savedBB=BB, savedBR=BR, savedBQ=BQ, savedBK=BK;
            int savedEP = get_ep_square();
            int savedCR = get_castle_rights();

            move_make(sq, toSq, sideToMove, -1);
            uint64_t n = perft(depth-1, 1-sideToMove);
            total += n;

            char uci[6]; move_to_uci(sq, toSq, uci);
            printf("%s: %llu\n", uci, (unsigned long long)n);

            // undo
            WP=savedWP; WN=savedWN; WB=savedWB; WR=savedWR; WQ=savedWQ; WK=savedWK;
            BP=savedBP; BN=savedBN; BB=savedBB; BR=savedBR; BQ=savedBQ; BK=savedBK;
            set_ep_square(savedEP);
            set_castle_rights(savedCR);
        }
    }
    printf("Total: %llu\n", (unsigned long long)total);
}
