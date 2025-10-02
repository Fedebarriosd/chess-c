#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "board.h"

#define BOARD 8

// ---------- Texturas ----------
typedef enum {
    TEX_WP, TEX_WN, TEX_WB, TEX_WR, TEX_WQ, TEX_WK,
    TEX_BP, TEX_BN, TEX_BB, TEX_BR, TEX_BQ, TEX_BK,
    TEX_COUNT
} PieceTex;
static Texture2D gPieceTex[TEX_COUNT];

// --- Sonidos ---
static Sound sndMove;
static Sound sndCapture;
static Sound sndCastle;
static Sound sndPromo;
static Sound sndCheck;

// ---------- Utilidades de coordenadas ----------
static inline void square_to_xy(int file, int rank, int SQ, int *x, int *y) {
    *x = file * SQ;
    *y = (7 - rank) * SQ; // rank=0 abajo
}
static inline void pixel_to_square(int mouseX, int mouseY, int SQ, int *file, int *rank) {
    int f = mouseX / SQ;
    int r_from_top = mouseY / SQ;  // 0 = arriba
    int r = 7 - r_from_top;        // invertimos para 0 = abajo
    if (f < 0 || f >= BOARD || r < 0 || r >= BOARD) { *file = -1; *rank = -1; }
    else { *file = f; *rank = r; }
}

// ---------- Tablero ----------
static inline Color square_color(int file, int rank, Color light, Color dark) {
    return ((file + rank) % 2 == 0) ? dark : light; // a1 oscura
}
static void draw_board(int SQ, Color light, Color dark) {
    for (int rank = 0; rank < BOARD; ++rank)
        for (int file = 0; file < BOARD; ++file) {
            int x, y; square_to_xy(file, rank, SQ, &x, &y);
            DrawRectangle(x, y, SQ, SQ, square_color(file, rank, light, dark));
        }
}

// ---------- Debug overlay ----------
static bool gShowDebug = false;
static const Color DBG_BG = {30,30,50,180};
static const Color DBG_FG = {220,230,255,255};

// ---------- Selección / turno ----------
static int gSelectedSq = -1;   // -1 = nada seleccionado
static int gSideToMove = 1;    // 1 blancas, 0 negras

// ---------- Game Over ----------
static bool gGameOver = false;
static char gGameOverMsg[64] = "";

// ---------- Carga de sprites ----------
static bool load_piece_textures(const char *baseDir) {
    char path[256];
    #define LOAD(slot, filename) do { \
        snprintf(path, sizeof(path), "%s/%s", baseDir, filename); \
        gPieceTex[slot] = LoadTexture(path); \
        if (gPieceTex[slot].id == 0) { TraceLog(LOG_ERROR, "No pude cargar %s", path); return false; } \
        SetTextureFilter(gPieceTex[slot], TEXTURE_FILTER_BILINEAR); \
    } while (0)

    LOAD(TEX_WP,"wP.png"); LOAD(TEX_WN,"wN.png"); LOAD(TEX_WB,"wB.png");
    LOAD(TEX_WR,"wR.png"); LOAD(TEX_WQ,"wQ.png"); LOAD(TEX_WK,"wK.png");
    LOAD(TEX_BP,"bP.png"); LOAD(TEX_BN,"bN.png"); LOAD(TEX_BB,"bB.png");
    LOAD(TEX_BR,"bR.png"); LOAD(TEX_BQ,"bQ.png"); LOAD(TEX_BK,"bK.png");
    #undef LOAD
    return true;
}
static void unload_piece_textures(void) {
    for (int i = 0; i < TEX_COUNT; ++i) if (gPieceTex[i].id) UnloadTexture(gPieceTex[i]);
}

// ---------- Mapear casilla -> índice de textura ----------
static int piece_index_at_tex(int sq) {
    uint64_t m = bit_at(sq);
    if (WP & m) return TEX_WP; if (WN & m) return TEX_WN; if (WB & m) return TEX_WB;
    if (WR & m) return TEX_WR; if (WQ & m) return TEX_WQ; if (WK & m) return TEX_WK;
    if (BP & m) return TEX_BP; if (BN & m) return TEX_BN; if (BB & m) return TEX_BB;
    if (BR & m) return TEX_BR; if (BQ & m) return TEX_BQ; if (BK & m) return TEX_BK;
    return -1;
}

// ---------- Animación de movimientos ----------
typedef struct {
    bool active;
    int fromSq, toSq;
    int texIdx;        // textura de la pieza que se mueve (capturada antes de mover)
    float t;           // 0..1
    float duration;    // segundos (ej: 0.18f)
} MoveAnim;

static MoveAnim gAnim = { false, -1, -1, -1, 0.0f, 0.18f };

// --- Segunda animación para la torre en enroques ---
static MoveAnim gAnimR = { false, -1, -1, -1, 0.0f, 0.18f };

// Flag para saber si hay que cambiar el turno cuando terminen TODAS las animaciones
static bool gPendingTurnSwitch = false;

// Easing cúbico sin libm (smoothstep cúbico)
static float easeInOutCubic(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);  // 3t^2 - 2t^3
}

// ---------- Dibujar piezas con proporción (omite destino si hay animación) ----------
static void draw_pieces_textured(int SQ) {
    const float scalePad = 0.90f; // 90% de la casilla
    for (int sq = 0; sq < 64; ++sq) {
        if ((gAnim.active && sq == gAnim.toSq) || (gAnimR.active && sq == gAnimR.toSq)) continue; // evitar duplicado

        int idx = piece_index_at_tex(sq);
        if (idx < 0) continue;

        int file = sq % 8, rank = sq / 8, x, y;
        square_to_xy(file, rank, SQ, &x, &y);

        Texture2D tex = gPieceTex[idx];
        float scale = scalePad * (float)SQ / (float)((tex.width > tex.height) ? tex.width : tex.height);
        float w = tex.width * scale, h = tex.height * scale;
        Rectangle src = (Rectangle){0,0,(float)tex.width,(float)tex.height};
        Rectangle dst = (Rectangle){ x + (SQ - w)*0.5f, y + (SQ - h)*0.5f, w, h };
        DrawTexturePro(tex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    }
}

// ---------- Promoción: UI ----------
typedef struct {
    bool active;
    int fromSq, toSq;
    int side; // 1 blancas, 0 negras
} PromotionUI;

static PromotionUI gPromo = (PromotionUI){ false, -1, -1, 1 };

// ¿el movimiento desde->hacia sería una promoción?
static inline bool would_promote(int fromSq, int toSq, int side) {
    int code = piece_code_at(fromSq); // 0=WP, 6=BP
    int toRank = toSq / 8;
    return (side==1 && code==0 && toRank==7) || (side==0 && code==6 && toRank==0);
}

// Mapeos de código de promoción para move_make()
static inline int promo_code_from_char(int side, char c) {
    if (side==1) { if (c=='q'||c=='Q') return 4; if (c=='r'||c=='R') return 3; if (c=='b'||c=='B') return 2; if (c=='n'||c=='N') return 1; }
    else         { if (c=='q'||c=='Q') return 10; if (c=='r'||c=='R') return 9; if (c=='b'||c=='B') return 8; if (c=='n'||c=='N') return 7; }
    return -1;
}

// Devuelve el índice de textura según side y pieza (Q,R,B,N)
static int promo_texture_index(int side, char piece) {
    if (side==1) { if (piece=='Q') return TEX_WQ; if (piece=='R') return TEX_WR; if (piece=='B') return TEX_WB; if (piece=='N') return TEX_WN; }
    else         { if (piece=='Q') return TEX_BQ; if (piece=='R') return TEX_BR; if (piece=='B') return TEX_BB; if (piece=='N') return TEX_BN; }
    return -1;
}

// Dibuja el modal y detecta clic/teclas
static int draw_and_pick_promotion(int SQ) {
    const int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, (Color){0,0,0,120});

    const int pad = 16, boxW = (int)(SQ * 4.2f), boxH = (int)(SQ * 1.8f);
    const int boxX = (W - boxW)/2, boxY = (H - boxH)/2;
    DrawRectangle(boxX, boxY, boxW, boxH, (Color){35,35,48,240});
    DrawRectangleLines(boxX, boxY, boxW, boxH, (Color){180,180,220,220});
    DrawText("Promocionar a (Q/R/B/N o clic)", boxX + pad, boxY + 8, 18, (Color){230,230,255,255});

    const char opts[4] = {'Q','R','B','N'};
    Rectangle rects[4];
    for (int i=0;i<4;i++){
        int slotW = (boxW - pad*2) / 4;
        int sx = boxX + pad + i*slotW, sy = boxY + 36;
        int sw = slotW - 6, sh = boxH - 36 - pad;
        rects[i] = (Rectangle){(float)sx,(float)sy,(float)sw,(float)sh};
        DrawRectangleRec(rects[i], (Color){50,50,66,255});
        DrawRectangleLinesEx(rects[i], 2, (Color){180,180,220,220});

        int texIdx = promo_texture_index(gPromo.side, opts[i]);
        if (texIdx >= 0) {
            Texture2D t = gPieceTex[texIdx];
            float maxSide = (rects[i].width < rects[i].height ? rects[i].width : rects[i].height) * 0.8f;
            float scale = maxSide / (float)((t.width > t.height) ? t.width : t.height);
            float w = t.width * scale, h = t.height * scale;
            float dx = rects[i].x + (rects[i].width - w)/2.0f;
            float dy = rects[i].y + (rects[i].height - h)/2.0f;
            DrawTexturePro(t, (Rectangle){0,0,(float)t.width,(float)t.height}, (Rectangle){dx,dy,w,h}, (Vector2){0,0}, 0, WHITE);
        }
        DrawText((const char[]){opts[i],0}, (int)(rects[i].x + rects[i].width/2 - 6),
                 (int)(rects[i].y + rects[i].height - 18), 18, (Color){230,230,255,255});
    }
    if (IsKeyPressed(KEY_Q)) return promo_code_from_char(gPromo.side, 'Q');
    if (IsKeyPressed(KEY_R)) return promo_code_from_char(gPromo.side, 'R');
    if (IsKeyPressed(KEY_B)) return promo_code_from_char(gPromo.side, 'B');
    if (IsKeyPressed(KEY_N)) return promo_code_from_char(gPromo.side, 'N');

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        for (int i=0;i<4;i++) if (CheckCollisionPointRec(m, rects[i])) return promo_code_from_char(gPromo.side, opts[i]);
    }
    return -1;
}

// ---------- Helpers: chequeo fin de partida ----------
static void check_game_over_after_turn_change(void) {
    bool noMoves = true;
    for (int sq = 0; sq < 64; ++sq) {
        if ((gSideToMove==1 && is_white_at(sq)) || (gSideToMove==0 && is_black_at(sq))) {
            uint64_t moves = gen_legal_moves_from(sq, gSideToMove);
            if (moves) { noMoves = false; break; }
        }
    }
    if (noMoves) {
        if (is_king_in_check(gSideToMove)) {
            snprintf(gGameOverMsg, sizeof(gGameOverMsg), "Jaque mate! %s gana",
                     gSideToMove ? "Negras" : "Blancas");
        } else {
            snprintf(gGameOverMsg, sizeof(gGameOverMsg), "Tablas por ahogado");
        }
        gGameOver = true;
    }
}

static uint64_t gMoveTargets = 0ULL;

int main(void) {
    const int W = 720, H = 720;
    const int SQ = W / BOARD;
    const Color COL_LIGHT = (Color){240,217,181,255};
    const Color COL_DARK  = (Color){181,136, 99,255};

    InitWindow(W, H, "Chess (C + raylib)");
    SetExitKey(KEY_NULL); // controlamos ESC nosotros

    InitAudioDevice();
    SetMasterVolume(1.0f);

    // Carga de sonidos (log si falla)
    sndMove    = LoadSound("assets/move.wav");
    if (sndMove.frameCount == 0) TraceLog(LOG_WARNING, "No pude cargar assets/move.wav");

    sndCapture = LoadSound("assets/capture.wav");
    if (sndCapture.frameCount == 0) TraceLog(LOG_WARNING, "No pude cargar assets/capture.wav");

    sndCastle  = LoadSound("assets/castle.wav");
    if (sndCastle.frameCount == 0) TraceLog(LOG_WARNING, "No pude cargar assets/castle.wav");

    sndPromo   = LoadSound("assets/promo.wav");
    if (sndPromo.frameCount == 0) TraceLog(LOG_WARNING, "No pude cargar assets/promo.wav");

    sndCheck   = LoadSound("assets/check.wav");
    if (sndCheck.frameCount == 0) TraceLog(LOG_WARNING, "No pude cargar assets/check.wav");


    if (!load_piece_textures("assets")) { CloseAudioDevice(); CloseWindow(); return 1; }
    SetTargetFPS(60);

    board_init_startpos();

    bool running = true;
    while (running && !WindowShouldClose()) {

        // --------- INPUT ---------
        if (IsKeyPressed(KEY_F3)) gShowDebug = !gShowDebug;

        // ESC: modal -> cierra modal; si no hay modal, salir
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (gPromo.active) gPromo.active = false;
            else running = false;
        }

        int mx = GetMouseX(), my = GetMouseY();
        int f=-1, r=-1; pixel_to_square(mx, my, SQ, &f, &r);
        int hoverSq = (f==-1 || r==-1) ? -1 : (r*8 + f);

        // Bloqueo de input si hay animación, promoción o game over
        bool inputLocked = gAnim.active || gAnimR.active || gPromo.active || gGameOver;

        // Clic izquierdo: seleccionar o mover
        if (!inputLocked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hoverSq != -1) {
            if (gSelectedSq == -1) {
                if ((gSideToMove==1 && is_white_at(hoverSq)) ||
                    (gSideToMove==0 && is_black_at(hoverSq))) {
                    gSelectedSq = hoverSq;
                    gMoveTargets = gen_legal_moves_from(gSelectedSq, gSideToMove);
                }
            } else {
                if ((gMoveTargets & bit_at(hoverSq)) && hoverSq != gSelectedSq) {
                    // ¿Promoción?
                    if (would_promote(gSelectedSq, hoverSq, gSideToMove)) {
                        gPromo = (PromotionUI){ true, gSelectedSq, hoverSq, gSideToMove };
                    } else {
                        // Animación de movimiento normal
                        int movingTex = piece_index_at_tex(gSelectedSq); // antes de mover

                        // ¿Es enroque?
                        bool isWhite = (gSideToMove == 1);
                        bool isKingMove = (movingTex == (isWhite ? TEX_WK : TEX_BK));
                        bool isCastleShort =
                            (isWhite && gSelectedSq == square_index(4,0) && hoverSq == square_index(6,0)) ||
                            (!isWhite && gSelectedSq == square_index(4,7) && hoverSq == square_index(6,7));
                        bool isCastleLong  =
                            (isWhite && gSelectedSq == square_index(4,0) && hoverSq == square_index(2,0)) ||
                            (!isWhite && gSelectedSq == square_index(4,7) && hoverSq == square_index(2,7));
                        bool isCastle = isKingMove && (isCastleShort || isCastleLong);

                        // ¿Captura normal en destino? (calcular ANTES de mover)
                        bool isCapture = (gSideToMove == 1) ? is_black_at(hoverSq) : is_white_at(hoverSq);

                        // ¿En-passant?
                        int codeFrom = piece_code_at(gSelectedSq); // 0=WP, 6=BP
                        bool isEP = false;
                        int epSq = get_ep_square(); // -1 si no hay
                        if ((codeFrom == 0 || codeFrom == 6) && epSq != -1 && hoverSq == epSq) {
                            isEP = true;
                        }

                        // Calcular de antemano la torre (move_make ya la mueve)
                        int rookFrom = -1, rookTo = -1, rookTex = -1;
                        if (isCastle) {
                            if (isWhite) {
                                if (isCastleShort) { rookFrom = square_index(7,0); rookTo = square_index(5,0); rookTex = TEX_WR; }
                                else               { rookFrom = square_index(0,0); rookTo = square_index(3,0); rookTex = TEX_WR; }
                            } else {
                                if (isCastleShort) { rookFrom = square_index(7,7); rookTo = square_index(5,7); rookTex = TEX_BR; }
                                else               { rookFrom = square_index(0,7); rookTo = square_index(3,7); rookTex = TEX_BR; }
                            }
                        }

                        if (move_make(gSelectedSq, hoverSq, gSideToMove, -1)) {
                            // Sonidos (usar info previa al movimiento)
                            if (isCastle) {
                                PlaySound(sndCastle);
                            } else if (isEP || isCapture) {
                                PlaySound(sndCapture);
                            } else {
                                PlaySound(sndMove);
                            }

                            // Animación del REY
                            gAnim = (MoveAnim){ true, gSelectedSq, hoverSq, movingTex, 0.0f, 0.18f };

                            // Si fue enroque, también animamos la TORRE
                            if (isCastle && rookFrom != -1) {
                                gAnimR = (MoveAnim){ true, rookFrom, rookTo, rookTex, 0.0f, 0.18f };
                            } else {
                                gAnimR.active = false;
                            }

                            // Cambiar turno al finalizar TODAS las animaciones
                            gPendingTurnSwitch = true;
                        }
                    }
                }
                gSelectedSq = -1;
                gMoveTargets = 0ULL;
            }
        }

        // Clic derecho: cancelar selección (si no hay modal/animación/game over)
        if (!inputLocked && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            gSelectedSq = -1;
            gMoveTargets = 0ULL;
        }

        // Avance de animaciones (rey y torre)
        if (gAnim.active) {
            gAnim.t += GetFrameTime() / gAnim.duration;
            if (gAnim.t >= 1.0f) { gAnim.t = 1.0f; gAnim.active = false; }
        }
        if (gAnimR.active) {
            gAnimR.t += GetFrameTime() / gAnimR.duration;
            if (gAnimR.t >= 1.0f) { gAnimR.t = 1.0f; gAnimR.active = false; }
        }

        // Cuando no quedan animaciones pendientes, cambiamos el turno y chequeamos mate/ahogado
        if (gPendingTurnSwitch && !gAnim.active && !gAnimR.active) {
            gPendingTurnSwitch = false;
            gSideToMove = 1 - gSideToMove;
            check_game_over_after_turn_change();

            // Sonido de jaque (al comenzar el turno en jaque)
            if (!gGameOver && is_king_in_check(gSideToMove)) {
                PlaySound(sndCheck);
            }
        }

        // --------- DIBUJO ---------
        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_board(SQ, COL_LIGHT, COL_DARK);
        draw_pieces_textured(SQ);

        // Capa de animación de la TORRE (enroque)
        if (gAnimR.active && gAnimR.texIdx >= 0) {
            int f0 = gAnimR.fromSq % 8, r0 = gAnimR.fromSq / 8;
            int f1 = gAnimR.toSq   % 8, r1 = gAnimR.toSq   / 8;

            int x0, y0, x1, y1;
            square_to_xy(f0, r0, SQ, &x0, &y0);
            square_to_xy(f1, r1, SQ, &x1, &y1);

            float t = easeInOutCubic(gAnimR.t);
            float cx = x0 + (x1 - x0) * t;
            float cy = y0 + (y1 - y0) * t;

            Texture2D tex = gPieceTex[gAnimR.texIdx];
            float scalePad = 0.90f;
            float scale = scalePad * (float)SQ / (float)((tex.width > tex.height) ? tex.width : tex.height);
            float w = tex.width * scale, h = tex.height * scale;

            Rectangle src = (Rectangle){0,0,(float)tex.width,(float)tex.height};
            Rectangle dst = (Rectangle){ cx + (SQ - w)*0.5f, cy + (SQ - h)*0.5f, w, h };
            DrawTexturePro(tex, src, dst, (Vector2){0,0}, 0, WHITE);
        }

        // Resaltar al rey si está en jaque
        if (!gGameOver && is_king_in_check(gSideToMove)) {
            int kingSq = -1;
            if (gSideToMove == 1) { // blancas
                for (int sq = 0; sq < 64; sq++) { if (WK & bit_at(sq)) { kingSq = sq; break; } }
            } else {
                for (int sq = 0; sq < 64; sq++) { if (BK & bit_at(sq)) { kingSq = sq; break; } }
            }
            if (kingSq != -1) {
                int kf = kingSq % 8, kr = kingSq / 8, x, y;
                square_to_xy(kf, kr, SQ, &x, &y);
                DrawRectangle(x, y, SQ, SQ, (Color){255, 0, 0, 80});     // overlay rojo translúcido
                DrawRectangleLines(x, y, SQ, SQ, (Color){200, 0, 0, 200}); // borde rojo
            }
        }

        // destinos válidos
        if (gMoveTargets && !gGameOver) {
            for (int sq = 0; sq < 64; ++sq) {
                if (gMoveTargets & bit_at(sq)) {
                    int tf = sq % 8, tr = sq / 8, x, y;
                    square_to_xy(tf, tr, SQ, &x, &y);
                    DrawCircle(x + SQ/2, y + SQ/2, SQ*0.20f, (Color){0,180,0,140});
                    DrawRectangle(x, y, SQ, SQ, (Color){0,255,0,30});
                }
            }
        }

        // selección
        if (gSelectedSq != -1 && !gGameOver) {
            int sfile = gSelectedSq % 8, srank = gSelectedSq / 8, x, y;
            square_to_xy(sfile, srank, SQ, &x, &y);
            DrawRectangle(x, y, SQ, SQ, (Color){0,200,255,60});
            DrawRectangleLines(x, y, SQ, SQ, (Color){0,120,200,200});
        }

        // hover
        if (!gGameOver && hoverSq != -1 && hoverSq != gSelectedSq) {
            int hfile = hoverSq % 8, hrank = hoverSq / 8, x, y;
            square_to_xy(hfile, hrank, SQ, &x, &y);
            DrawRectangle(x, y, SQ, SQ, (Color){255,255,0,40});
        }

        // debug
        if (gShowDebug) {
            DrawRectangle(12, 12, 150, 80, DBG_BG);
            if (f!=-1) DrawText(TextFormat("file=%d  rank=%d", f+1, r+1), 20, 18, 20, DBG_FG);
            DrawText(gSideToMove ? "Turno: Blancas" : "Turno: Negras", 20, 40, 18, DBG_FG);
            DrawText(is_king_in_check(gSideToMove) ? "¡Jaque!" : "", 20, 58, 18, RED);
            DrawText(gGameOver ? "GAME OVER" : "", 20, 72, 18, ORANGE);
        }

        // Modal de promoción
        if (gPromo.active && !gGameOver) {
            int promoCode = draw_and_pick_promotion(SQ);
            if (promoCode != -1) {
                if (move_make(gPromo.fromSq, gPromo.toSq, gPromo.side, promoCode)) {
                    PlaySound(sndPromo);
                    gSideToMove = 1 - gSideToMove;
                    check_game_over_after_turn_change();
                    if (!gGameOver && is_king_in_check(gSideToMove)) {
                        PlaySound(sndCheck);
                    }
                }
                gPromo.active = false;
            }
        }

        // Overlay de Game Over
        if (gGameOver) {
            int WW = GetScreenWidth(), HH = GetScreenHeight();
            DrawRectangle(0,0,WW,HH,(Color){0,0,0,180});
            int tw = MeasureText(gGameOverMsg, 40);
            DrawText(gGameOverMsg, (WW - tw)/2, HH/2 - 22, 40, RAYWHITE);
            DrawText("ESC: salir",
                     (WW - MeasureText("ESC: salir", 18))/2,
                     HH/2 + 28, 18, LIGHTGRAY);
        }

        EndDrawing();
    }

    // Descarga
    UnloadSound(sndMove);
    UnloadSound(sndCapture);
    UnloadSound(sndCastle);
    UnloadSound(sndPromo);
    UnloadSound(sndCheck);
    CloseAudioDevice();

    unload_piece_textures();
    CloseWindow();
    return 0;
}
