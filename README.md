# Catálogo musical indexado y confiable

Laboratorio de **Estructura de Datos II** desarrollado en **C++20**. El proyecto
implementa un catálogo musical almacenado en un archivo binario de registros de
longitud variable, con índices para realizar búsquedas eficientes y validaciones
para detectar datos dañados o referencias inconsistentes.

> **Estudiante:** Reemplazar con tu nombre completo  
> **Curso:** Estructura de Datos II  
> **Lenguaje:** C++20

## Características

- Lectura segura de registros binarios mediante offsets.
- Validación de magic, versión, longitud, truncamiento y CRC-32.
- Índice primario ordenado: `label_id -> offset`.
- Búsqueda binaria manual en `O(log n)`.
- Índice secundario invertido: `composer -> lista<label_id>`.
- Detección de claves y offsets duplicados.
- Auditoría de consistencia entre el índice y el archivo de datos.
- Intersección de listas ordenadas en `O(n + m)` como funcionalidad adicional.

## Funcionamiento general

```text
catalog.psv
    |
    v
catalog_generate
    |
    v
catalog.bin
    |
    +--> Índice primario: label_id -> offset
    |
    +--> Índice secundario: composer -> lista<label_id>
    |
    +--> Verificación de integridad y consistencia
```

El archivo binario conserva el orden original de los registros. El índice
primario se ordena de manera independiente por `label_id`, por lo que permite
buscar rápidamente sin reorganizar los datos físicos.

Cada registro contiene:

```text
magic | version | payload_length | payload | crc32
```

El payload almacena:

```text
label_id | composer | title
```

## Funciones principales

| Función | Propósito |
|---|---|
| `read_record_at` | Lee y valida un registro desde un offset específico. |
| `build_primary_index` | Construye y ordena el índice `label_id -> offset`. |
| `find_offset` | Busca un `label_id` mediante búsqueda binaria manual. |
| `find_record` | Busca una clave y recupera su registro validado. |
| `build_composer_index` | Construye el índice `composer -> lista<label_id>`. |
| `find_by_composer` | Busca las claves asociadas a un compositor. |
| `verify_primary_index` | Audita la estructura del índice y sus referencias. |
| `intersect_sorted` | Interseca dos listas ordenadas sin duplicados. |

## Estructura del proyecto

```text
IndexedCatalog_Starter/
|-- CMakeLists.txt
|-- README.md
|-- data/
|   `-- catalog.psv
|-- include/
|   |-- binary_io.hpp
|   |-- catalog.hpp
|   |-- catalog_codec.hpp
|   |-- crc32.hpp
|   `-- index_io.hpp
|-- src/
|   |-- binary_io.cpp
|   |-- catalog.cpp
|   |-- catalog_codec.cpp
|   |-- crc32.cpp
|   |-- index_io.cpp
|   `-- main.cpp
|-- tests/
|   |-- student_tests.cpp
|   `-- tests.cpp
|-- third_party/
|   `-- doctest/
`-- tools/
    |-- corrupt_file.cpp
    `-- generate_catalog.cpp
```

## Requisitos

- CMake 3.20 o superior.
- Compilador compatible con C++20.
- Visual Studio 2022, GCC o Clang.

## Compilación

Desde la carpeta raíz del proyecto:

```bash
cmake -S . -B build
cmake --build build
```

En Windows con Visual Studio:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Ejecución de pruebas

En Linux o con un generador de una sola configuración:

```bash
ctest --test-dir build --output-on-failure
```

En Windows con Visual Studio:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

También puede ejecutarse directamente:

```powershell
.\build\Debug\catalog_tests.exe
```

## Uso de la aplicación

Los siguientes ejemplos usan las rutas generadas por Visual Studio en modo
`Debug`. En Linux, normalmente los ejecutables se encuentran directamente en
`build/`.

### 1. Generar el catálogo binario

```powershell
.\build\Debug\catalog_generate.exe data\catalog.psv data\catalog.bin
```

### 2. Construir el índice primario

```powershell
.\build\Debug\lab2_catalog.exe build data\catalog.bin data\catalog.idx
```

### 3. Buscar por `label_id`

```powershell
.\build\Debug\lab2_catalog.exe find data\catalog.bin data\catalog.idx DG18807
```

Ejemplo de salida:

```text
DG18807 | BEETHOVEN | SYMPHONY NO. 9
```

### 4. Buscar por compositor

```powershell
.\build\Debug\lab2_catalog.exe composer data\catalog.bin data\catalog.idx BEETHOVEN
```

### 5. Verificar la consistencia

```powershell
.\build\Debug\lab2_catalog.exe verify data\catalog.bin data\catalog.idx
```

Un catálogo consistente debe producir un resultado similar a:

```text
Entradas revisadas: 10
Referencias legibles y coincidentes: 10
Problemas: 0
```

## Simulación de corrupción

La herramienta `catalog_corrupt` permite comprobar que las validaciones detectan
archivos dañados.

Invertir un bit:

```powershell
.\build\Debug\catalog_corrupt.exe flip data\catalog.bin data\catalog.corrupt 20
.\build\Debug\lab2_catalog.exe verify data\catalog.corrupt data\catalog.idx
```

Truncar una copia del catálogo:

```powershell
.\build\Debug\catalog_corrupt.exe truncate data\catalog.bin data\catalog.corrupt 3
.\build\Debug\lab2_catalog.exe verify data\catalog.corrupt data\catalog.idx
```

## Integridad física y consistencia lógica

La **integridad física** comprueba que los bytes estén completos y no hayan sido
alterados. Algunos ejemplos son un payload truncado, un CRC ausente o un CRC que
no coincide.

La **consistencia lógica** comprueba que las estructuras y referencias tengan
sentido para el catálogo. Algunos ejemplos son claves duplicadas, un índice
desordenado, offsets repetidos o una clave del índice que no coincide con el
registro recuperado.

Un CRC válido demuestra que el payload no cambió, pero no garantiza que el índice
apunte al registro correcto.

## Complejidades

| Operación | Complejidad |
|---|---|
| Lectura de un registro | `O(p)`, donde `p` es el tamaño del payload |
| Construcción del índice primario | `O(n log n)` |
| Búsqueda primaria | `O(log n)` más la lectura de un registro |
| Construcción del índice por compositor | `O(n log n)` |
| Búsqueda por compositor | `O(log c)` |
| Auditoría del índice | `O(n log n)` más las lecturas |
| Intersección de listas ordenadas | `O(n + m)` |

## Pruebas adicionales

Las pruebas de `tests/student_tests.cpp` incluyen:

- Construcción del índice desde un archivo vacío.
- Detección de un header truncado.
- Omisión de una referencia cuya clave no coincide.
- Detección simultánea de índice desordenado, clave duplicada y offset duplicado.
- Intersección ordenada sin valores repetidos.

<<<<<<< HEAD
## Archivos que no deben subirse

Antes de entregar o publicar, se recomienda excluir:

```text
build/
*.bin
*.idx
*.corrupt
*.truncated
```

## Autor

**Reemplazar con tu nombre completo**
=======
>>>>>>> 08c8284220d59980c36a8b46a53e98c196081c24
