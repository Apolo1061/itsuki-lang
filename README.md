<p align="center">
  <img src="img/itsuki_logo.png" alt="Itsuki Programming Language" width="160">
</p>

<h1 align="center">Itsuki Programming Language</h1>

<p align="center">
  <a href="https://discord.gg/K6SEpB7Pf5">
    <img src="https://img.shields.io/badge/Discord-Unirse%20a%20la%20Comunidad-7289DA?logo=discord&logoColor=white" alt="Discord">
  </a>
  <img src="https://img.shields.io/badge/version-5.0.0-blue.svg" alt="Versión">
  <img src="https://img.shields.io/badge/arquitectura-x86_32-brightgreen.svg" alt="Arquitectura">
  <img src="https://img.shields.io/badge/plataforma-Windows%20%7C%20Linux-orange.svg" alt="Plataforma">
  <img src="https://img.shields.io/badge/licencia-MIT-green.svg" alt="Licencia">
</p>

**Itsuki** es un lenguaje de programacion compilado y de alto rendimiento que genera codigo ensamblador nativo x86. Diseñado para ofrecer control de bajo nivel con gran claridad incluye un runtime nativo autocontenido, un preprocesador C completo, interoperabilidad directa con C, c++ y un **gestor de paquetes integrado** basado en GitHub.

---

## Características Principales

*  **Generación de Codigo Nativo**: Compila directamente a ensamblador x86 de 32 bits de alto rendimiento.
*  **Gestor de Paquetes Integrado**: Instala y actualiza librerías directamente desde GitHub con `itsuki install <url|usuario/repo>`.
*  **Sistema de Inclusión Diferenciado**:
  * `<libreria.huki>`: Búsqueda estricta en el sistema (`~/.itsuki/packages/`) y librerías integradas.
  * `"archivo.huki"`: Búsqueda local en el proyecto o ruta exacta especificada.
  * *Descubrimiento automático*: Al incluir una cabecera `.huki`, su implementación compañera `.suki` se compila y enlaza automáticamente.
*  **Interoperabilidad Nativa con C**: Llama a funciones de C, enlaza archivos `.c` directamente y utiliza encabezados C en tu proyecto sin wrappers complejos.
*  **Programación Estructurada y Orientada a Objetos**: Soporta funciones, punteros, arreglos, structs anidados, clases con herencia, constructores y métodos.
*  **Preprocesador Potente**: Compatible con `#include`, `#import`, `#pragma once`, `#define` (con macros funcionales y variádicas) y directivas condicionales (`#if`, `#ifdef`, `#elif`, `#else`, `#endif`).
*  **Ensamblador en Línea**: Bloques `__asm__("...")` para optimizaciones críticas de hardware.

---

## 🚀 Instalación Rápida

### En Windows
Accede a la carpeta `windows` y ejecuta el instalador que prefieras:

1. **Script Batch (CMD)**:
   Haz doble clic en `windows\install.bat` o ejecútalo desde tu consola:
   ```cmd
   cd windows
   install.bat
   ```
2. **PowerShell**:
   ```powershell
   cd windows
   powershell -ExecutionPolicy Bypass -File install.ps1
   ```

*Itsuki se instalará en `%USERPROFILE%\.itsuki\bin` y se agregará permanentemente a tu `PATH`.*

### En Linux
Accede a la carpeta `linux` y ejecuta:

```bash
cd linux
bash install.sh
```
*(O con `sudo bash install.sh` para una instalación en todo el sistema).*

---

## 💻 Guía de Inicio Rápido

### Tu primer programa (`hola.suki`)

Crea un archivo llamado `hola.suki`:

```itsuki
#include <fundamental.suki>

fn main() -> void {
    printf("¡Hola, Mundo desde Itsuki!\n");
}
```

### Ejecutarlo directamente:
```bash
itsuki run hola.suki
```

### Compilar un binario ejecutable:
```bash
itsuki build hola.suki -o mi_programa
```

---

## 📦 Gestor de Paquetes

Itsuki permite descargar, instalar y gestionar librerías externas alojadas en GitHub. Los paquetes se almacenan globalmente en el sistema (`~/.itsuki/packages/`), quedando disponibles para cualquier proyecto.

### Instalar una librería
```bash
# Mediante URL completa de Git:
itsuki install https://github.com/apolo1061/libreria.git

# O formato corto usuario/repositorio:
itsuki install apolo1061/libreria
```

### Listar librerías instaladas
```bash
itsuki list
```

### Desinstalar una librería
```bash
itsuki uninstall libreria
```

### Usar la librería en tu código
```itsuki
#include <fundamental.suki>
#include <libreria.huki>     // Se importa automáticamente desde ~/.itsuki/packages/

fn main() -> void {
    // Funciones de la librería instalada
}
```

---

## 📖 Sintaxis del Lenguaje

### Variables y Tipos
```itsuki
var x: int = 10;
var pi: int = 31416; // Valores en punto fijo o enteros
var mensaje: char* = "Cadena de texto";
var activo: int = 1;
```

### Funciones
```itsuki
fn sumar(a: int, b: int) -> int {
    return a + b;
}

fn saludar(nombre: char*) -> void {
    printf("Hola, %s\n", nombre);
}
```

### Estructuras y Punteros
```itsuki
struct Punto {
    var x: int;
    var y: int;
};

fn mover(p: Punto*, dx: int, dy: int) -> void {
    p->x += dx;
    p->y += dy;
}
```

### Clases y Métodos
```itsuki
class Contador {
    var valor: int;

public:
    fn Contador(inicio: int) -> void {
        this.valor = inicio;
    }

    fn incrementar() -> void {
        this.valor++;
    }

    fn obtener() -> int {
        return this.valor;
    }
};
```

### Interoperabilidad con C
Puedes llamar funciones de C declarándolas con `extern` y pasando los archivos `.c` al compilar:

```itsuki
// main.suki
#include <fundamental.suki>

extern fn funcion_en_c(a: int, b: int) -> int;

fn main() -> void {
    var res: int = funcion_en_c(10, 20);
    printf("Resultado de C: %d\n", res);
}
```

Compilación directa conjunta:
```bash
itsuki run main.suki codigo_c.c
```

---

## 📂 Estructura del Repositorio

```text
itsuki/
├── img/                   # Logotipo oficial del lenguaje
├── linux/                 # Compilador e instalador nativo para Linux
│   ├── headers/           # Cabeceras C (.h)
│   ├── lib/               # Runtime y librería estándar (<fundamental>, <math>, <string>, <c>)
│   ├── src/               # Implementación en C (.c)
│   ├── Makefile           # Makefile oficial
│   ├── build.sh           # Script de compilación rápida
│   └── install.sh         # Instalador automático para Linux (Bash)
├── windows/               # Compilador e instalador nativo para Windows (Win32)
│   ├── headers/           # Cabeceras C (.h)
│   ├── lib/               # Runtime y librería estándar
│   ├── src/               # Implementación en C (.c)
│   ├── build.bat          # Script de compilación rápida (MinGW)
│   ├── install.bat        # Instalador automático para Windows (Batch)
│   └── install.ps1        # Instalador automático para Windows (PowerShell)
├── .gitignore             # Filtro de archivos para Git
├── LICENSE                # Licencia de código abierto MIT
└── README.md              # Documentación oficial
```

---