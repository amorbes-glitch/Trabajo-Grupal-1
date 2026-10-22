# EL BOSQUE PERDIÓ SUS SOMBRAS

Videojuego educativo en C++ para niños de Educación Inicial (3 a 5 años). El
jugador ayuda a nueve animales a encontrar sus siluetas en tres niveles.

## Requisitos

- Windows 10 o posterior
- Un compilador C++17 para Windows (por ejemplo, MinGW-w64) o CMake con Visual Studio

El juego usa Win32 y GDI para los gráficos, así que no requiere instalar
bibliotecas adicionales.

## Compilar con MinGW-w64

Desde la carpeta del proyecto:

```powershell
g++ -std=c++17 -O2 -municode main.cpp -o ElBosquePerdioSusSombras.exe -lgdi32 -lwinmm
```



## Compilar con CMake

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Cómo se juega

- Selecciona **JUGAR** para iniciar o **INSTRUCCIONES** para leer las reglas.
- Haz clic en una de las tres siluetas para encontrar la sombra del animal.
- Al acertar, ganas una estrella y el juego avanza automáticamente.
- Si te equivocas, puedes intentarlo de nuevo sin perder estrellas.
- Completa los tres niveles para devolver la magia al bosque.

Los niveles incluyen león, conejo y elefante; jirafa, mono y mariposa; y
cocodrilo, búho y zorro.
