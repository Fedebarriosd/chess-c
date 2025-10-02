# Chess C + raylib

Ajedrez en C con interfaz gráfica usando [raylib](https://www.raylib.com/).  
Incluye selección con clic, animaciones de movimiento (incl. enroque), promoción con modal, resaltado de movimientos, overlay de jaque/jaque mate y sonidos.

---

## Requisitos

- **CMake** ≥ 3.16
- Compilador C (GCC/Clang en Linux, MinGW o MSVC en Windows)
- **raylib**
- Carpeta `assets/` con:
    - Piezas PNG (`wP.png`, `bP.png`, …, `wK.png`, `bK.png`)
    - Sonidos WAV (`move.wav`, `capture.wav`, `castle.wav`, `promo.wav`, `check.wav`)
        - ⚠️ **Formato recomendado:** WAV **PCM 16-bit**, 44.1 kHz o 48 kHz

Estructura esperada del repo:
chess-c/
├─ CMakeLists.txt
├─ src/
│ ├─ main.c
│ ├─ board.c
│ └─ board.h
└─ assets/
├─ wP.png … bK.png
├─ move.wav capture.wav castle.wav promo.wav check.wav

---

## Instalación y ejecución

### Linux

#### Arch
```bash
sudo pacman -S --needed cmake make gcc raylib
git clone https://github.com/Fedebarriosd/chess-c.git
cd chess-c
cmake -B build -S .
cmake --build build -j
./build/chess
```

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y cmake build-essential libraylib-dev
git clone https://github.com/Fedebarriosd/chess-c.git
cd chess-c
cmake -B build -S .
cmake --build build -j
./build/chess
```

> Si libraylib-dev no está disponible en tu distro, instala raylib manualmente:
https://github.com/raysan5/raylib

### Windows
#### Opción A: MSYS2 + MinGW (simple y rápido)
1. Instala [MSYS2](https://www.msys2.org/)
2. Abre MSYS2 MinGW 64-bit y ejecuta:
```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make mingw-w64-x86_64-raylib git
git clone https://github.com/Fedebarriosd/chess-c.git
cd chess-c
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build -j
./build/chess.exe
```

#### Opción B: Visual Studio (MSVC) + vspkg
1. Instala Visual Studio con Desktop development with C++.
2. Instala vcpkg y raylib:
```powershell
.\vcpkg install raylib:x64-windows
```
3. Configura CMake con el toolchain de vcpkg (o integra vcpkg como triplet por defecto).
4. Compila y ejecuta desde Visual Studio.

---

## Controles
- Click izquierdo (M1): seleccionar y mover pieza.
- Click derecho (M2): cancelar selección.
- F3: mostrar / ocultar debug.
- ESC: cerrar juego / cancelar promoción

---

## Troubleshooting
- No se escuchan sonidos: convertir WAV a formato PCM 16-bit con ffmpeg (recomendado).
```bash
ffmpeg -i input.wav -acodec pcm_s16le -ar 44100 output.wav -y
```
- El juego no encuentra assets: asegurarse de ejecutar el binario desde la carpeta root de clonado del repo o copiar `assets/` junto al ejecutable.

---

## Créditos

Proyecto desarrollado por **Fede Barrios** ([@fedebarriosd](https://github.com/fedebarriosd))

---

## Licencia
Este proyecto se distribuye bajo la **GNU General Public License v3.0**.  
Podés usarlo, modificarlo y redistribuirlo, pero cualquier trabajo derivado debe mantener la misma licencia.
