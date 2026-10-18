<p align="center">
  <img src="img/itsuki_logo.png" alt="Itsuki Programming Language" width="160">
</p>

<h1 align="center">Itsuki Programming Language</h1>

<p align="center">
  <a href="https://discord.gg/K6SEpB7Pf5">
    <img src="https://img.shields.io/badge/Discord-Unirse%20a%20la%20Comunidad-7289DA?logo=discord&logoColor=white" alt="Discord">
  </a>
  <img src="https://img.shields.io/badge/version-5.5.0-blue.svg" alt="Versión">
  <img src="https://img.shields.io/badge/arquitectura-x86_32%20%7C%20x86_64-brightgreen.svg" alt="Arquitectura">
  <img src="https://img.shields.io/badge/plataforma-Windows%20%7C%20Linux-orange.svg" alt="Plataforma">
  <img src="https://img.shields.io/badge/licencia-MIT-green.svg" alt="Licencia">
</p>

**Itsuki** es un lenguaje de programación compilado y de alto rendimiento que genera código ensamblador nativo **x86_64** y **x86_32**. Diseñado para ofrecer control de bajo nivel con gran claridad, incluye soporte para números de punto flotante (`float`, `double`) mediante SSE2, diagnósticos de error modernos estilo Rust/Clang, un optimizador por mirilla (peephole optimizer), runtime nativo autocontenido, preprocesador C completo, interoperabilidad directa con C/C++ y un **gestor de paquetes integrado** basado en GitHub.

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

*Itsuki se instalará en `%USERPROFILE%\.itsuki\bin` y se agregará permanentemente a tu `PATH`. Para desinstalar: `install.bat --uninstall` o ejecuta `%USERPROFILE%\.itsuki\uninstall.bat`.*

### En Linux
Accede a la carpeta `linux` y ejecuta:

```bash
cd linux
bash install.sh
```
*(No requiere permisos de superusuario ni `sudo`; se instala 100% en tu espacio de usuario `~/.itsuki/` y añade la ruta a tu `PATH`).*

> **Para desinstalar en Linux**:
> Ejecuta `bash install.sh --uninstall` o simplemente `~/.itsuki/uninstall.sh`.

---

## 💻 Guía de Inicio Rápido

### Tu primer programa (`hola.suki`)

Crea un archivo llamado `hola.suki`:

```c
#include <fundamental.huki>

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

### Diagnóstico de arquitectura del entorno:
```bash
itsuki doctor
```
*Muestra la arquitectura de la PC (64 o 32 bits), la versión de GCC en uso, los flags de enlace configurados y el estado del soporte multilib.*

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
```c
#include <fundamental.huki>
#include <libreria.huki>     // Se importa automáticamente desde ~/.itsuki/packages/

fn main() -> void {
    // Funciones de la librería instalada
}
```

---

## 📖 Sintaxis del Lenguaje

### Variables y Tipos
```c
var x: int = 10;
var f: float = 3.14159;      // Punto flotante de 32 bits (SSE2)
var d: double = 2.718281828; // Punto flotante de 64 bits (SSE2)
var big: int64 = 9223372036854775807;
var mensaje: char* = "Cadena de texto";
var activo: bool = true;
```

### Operaciones de Punto Flotante
```c
using namespace fundamental;

fn calcular_area(radio: float) -> float {
    var pi: float = 3.141592;
    return pi * radio * radio;
}

fn main() -> int {
    var radio: float = 5.5;
    var area: float = calcular_area(radio);
    println("Area del circulo:");
    println(area);
    return 0;
}
```

### Funciones
```c
fn sumar(a: int, b: int) -> int {
    return a + b;
}

fn saludar(nombre: char*) -> void {
    printf("Hola, %s\n", nombre);
}
```

### Estructuras y Punteros
```c
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
```c
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

```c
// main.suki
#include <fundamental.huki>

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
├── examble/               # Ejemplos listos para compilar de las librerías
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

##  Contribuidores y Agradecimientos

¡Un enorme agradecimiento a todas las personas que contribuyen y hacen posible el desarrollo y crecimiento de **Itsuki Programming Language**

<p align="center">
  <a href="https://github.com/Apolo1061/itsuki-lang/graphs/contributors">
    <img src="https://img.shields.io/github/contributors/Apolo1061/itsuki-lang?style=for-the-badge&logo=github&color=34D058&label=Colaboradores" alt="Número de Colaboradores" />
  </a>
</p>

<p align="center">
  <a href="https://github.com/Apolo1061/itsuki-lang/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Apolo1061/itsuki-lang" alt="Colaboradores de Itsuki" />
  </a>
</p>



### 🤝 ¿Quieres contribuir al proyecto?
¡Toda ayuda es bienvenida! Puedes colaborar de diversas maneras:
* 🐛 **Reportar fallos o bugs**: Abre un [Issue en GitHub](https://github.com/Apolo1061/itsuki-lang/issues).
* 💡 **Proponer nuevas características**: Sugerencias de sintaxis, librerías estándar o mejoras en el compilador.
* 🛠️ **Enviar Pull Requests**: Abre un [Pull Request](https://github.com/Apolo1061/itsuki-lang/pulls) con optimizaciones, correcciones o nuevas bibliotecas.

---

## 📄 Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Para más detalles, consulta el archivo [LICENSE](LICENSE).