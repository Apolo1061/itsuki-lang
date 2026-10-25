# Itsuki

Itsuki es un lenguaje compilado que genera ensamblador nativo x86-64/x86-32. Su sintaxis es familiar si venís de C o C++, pero con keywords propias (`fn`, `var`, `namespace` explícito) e interoperabilidad directa con C.

> Esta guía asume que ya sabés programar.

```itsuki
#include <fundamental.huki>

fn main() -> void {
    printf("¡Hola, Itsuki!\n");
}
```

Compilalo y ejecutalo con el CLI:

```bash
itsuki run hola.suki        # compila y ejecuta
itsuki build hola.suki -o hola  # genera un binario
```

## Comentarios

```itsuki
// Comentario de una línea.

/* Comentario
   multilínea. */
```

## Variables y constantes

Las variables se declaran con `var`, las constantes con `const`:

```itsuki
var x: int = 10;        // declaración con tipo explícito
var y = 20;             // inferida a partir del valor inicial
const MAX: int = 100;   // constante
var f: float = 3.14f;   // sufijo f para float
var d: double = 2.718281828;
var c: char = 'A';
var ok: bool = true;
var nada: int* = null;   // null, NULL y nullptr son equivalentes
```

## Tipos primitivos

| Enteros | Flotantes | Otros |
| --- | --- | --- |
| `int8`, `int16`, `int`, `int64` | `float` (SSE2) | `char` |
| `uint8`, `uint16`, `uint32`, `uint64` | `double` | `bool`, `void`, `string` |

Literales numéricos: decimales (`42`), hexadecimales (`0xFF`), flotantes con punto, notación científica (`1e10`) y sufijo `f` (`1.5f`).

## Operadores

```itsuki
// Aritméticos
a + b;
a - b;
a * b;
a / b;
a % b;

a++;
a--;
++a;
--a;

// Comparación
a == b;
a != b;
a < b;
a > b;
a <= b;
a >= b;

// Lógicos
a && b;
a || b;
!a;

// Bit a bit
a & b;
a | b;
a ^ b;
~a;
a << b;
a >> b;

// Asignación
x = b;
x += b;
x -= b;
x *= b;
x /= b;
x %= b;
x &= b;
x |= b;
x ^= b;
x <<= b;
x >>= b;

// Otros
a ? b : c;      // condicional ternario
obj->campo;     // acceso en puntero a struct
ns::nombre;     // resolución de namespace
```

## Estructuras de control

```itsuki
// Condicionales: if / else if / else
if (x > 10) {
    printf("grande\n");
} else if (x > 5) {
    printf("medio\n");
} else {
    printf("chico\n");
}

// while
while (x > 0) {
    x--;
}

// do-while: el cuerpo se ejecuta al menos una vez
do {
    x++;
} while (x < 10);

// for clásico de C: init; condición; actualización
for (var i: int = 0; i < 10; i++) {
    printf("%d\n", i);
}

// switch / case / default
switch (x) {
    case 1:
        printf("uno\n");
        break;
    case 2:
    case 3:
        printf("dos o tres\n");
        break;
    default:
        printf("otro\n");
        break;
}

// break y continue
while (true) {
    if (x == 5) break;
    if (x % 2 == 0) continue;
    x++;
}
```

## Funciones

Se declaran con `fn`; el tipo de retorno se indica con `->`. El tipo por defecto de retorno es `void`:

```itsuki
fn sumar(a: int, b: int) -> int {
    return a + b;
}

fn saludar() -> void {
    printf("hola\n");
}

// Prototipo (declaración sin cuerpo) — típico en archivos .huki
fn restar(a: int, b: int) -> int;

// Modificadores
inline fn duplicar(x: int) -> int { return x * 2; }
static fn contador() -> int { ... }
extern fn de_c(x: int) -> int;   // definida en otro lado (interop C)
```

## Punteros y arreglos

Sintaxis tipo C:

```itsuki
var p: int* = &x;         // puntero y dirección
var y: int = *p;          // desreferencia
var matriz: int**;        // puntero doble

var nums: int[10];        // arreglo de tamaño fijo
nums[0] = 1;

var tam: int[constante];  // el tamaño puede ser una constante de enum
```

## struct, union, enum y typedef

```itsuki
struct Punto {
    var x: int;
    var y: int;
};

struct Punto origen;              // uso
fn mover(p: struct Punto*) -> void;  // tipo struct en firma

union Dato {
    var como_int: int;
    var como_float: float;
};

enum Color {
    ROJO, VERDE, AZUL            // constantes evaluadas en compile-time
};

enum Niveles {
    BAJO = 1,
    MEDIO = 5,
    ALTO = 10
};

typedef int entero;               // alias de tipo
typedef struct Punto Punto_t;
var z: entero = 3;
```

## Namespaces

Todo lo declarado vive en namespaces; el acceso es con `::`:

```itsuki
namespace geometria {
    struct Punto { var x: int; var y: int; };
    fn origen() -> struct Punto;
}

var p: geometria::Punto;
geometria::origen();

// using importa los nombres al scope actual
using geometria::Punto;
var q: Punto;

// Encadenado
var s: fundamental::string;
```

## Clases

Itsuki también tiene clases con herencia y control de acceso:

```itsuki
class Animal {
public:
    fn hacer_sonido() -> void { printf("...\n"); }
};

class Perro : Animal {
public:
    fn hacer_sonido() -> void { printf("guau\n"); }
};
```

## Templates

Genéricos estilo C++, instanciados en compile-time:

```itsuki
template <typename T>
struct Par {
    var primero: T;
    var segundo: T;
};

var par: Par<int>;      // instancia con tipo concreto

template <typename T>
fn primero_de(p: Par<T>*) -> T {
    return p->primero;
}
```

## Ensamblador en línea

```itsuki
__asm__ ("mov rax, 42");
```

Para interoperar con C, usá `extern fn` (definida en otro lado) o incluí `c.huki`, que expone la libc:

```itsuki
extern fn version_c() -> int;

#include <c.huki>
fn main() -> void {
    var f: void* = fopen("datos.txt", "r");
    fclose(f);
}
```

## Módulos y archivos de cabecera (.huki)

Los headers de Itsuki usan la extensión **`.huki`** y se incluyen con `#include`. Las implementaciones van en `.suki`:

```itsuki
// matematicas.huki — solo declaraciones
#pragma once
#include <fundamental.huki>

namespace matematicas {
    fn es_primo(n: int) -> int;
}
```

```itsuki
// matematicas.suki — implementación
#include "matematicas.huki"

namespace matematicas {
    fn es_primo(n: int) -> int {
        if (n < 2) { return 0; }
        for (var i: int = 2; i * i <= n; i++) {
            if (n % i == 0) { return 0; }
        }
        return 1;
    }
}
```

Headers estándar que trae la instalación (en `lib/`):

- **`fundamental.huki`** — core: memoria (`malloc`, `free`), strings estilo C (`strlen`, `strcmp`), conversión (`atoi`, `itoa`), matemática básica (`abs`, `min`, `max`, `sqrt`), E/S (`printf`, `getchar`), archivos (`file_read_all`, `file_write_all`) y el struct dinámico `Vector`.
- **`math.huki`** — matemática extendida en el namespace `math`: `pow`, `gcd`, `factorial`, `is_prime`, constantes escaladas (`PI_APPROX`), trigonometría entera.
- **`string.huki`** — strings dinámicos tipo "clase": `struct String`, `string_new`, `string_append`, `string_equals`, `string_c_str`.
- **`c.huki`** — binding directo a la libc vía `extern "C"`: `fopen`, `fread`, `sprintf`, `system`, `getenv`, `qsort`.

Además existe `#import`, que el gestor de paquetes integrado resuelve desde GitHub.

```itsuki
#include <fundamental.huki>   // header local de la instalación
#include "mi_modulo.huki"     // header propio del proyecto
#import "usuario/repo"        // paquete remoto vía gestor integrado
```

## Diagnósticos

Los errores de sintaxis se reportan estilo Rust/Clang, con posición (línea/columna), el fragmento culpable subrayado y una sugerencia concreta ("coloca un punto y coma ';' al final de la instrucción").

## Listo

Tenés lo esencial. Para profundizar, el código fuente del compilador (en `linux/src/` o `windows/src/`) es la referencia final: `lexer.c` define todas las palabras clave, `parser.c` la gramática aceptada.