# Laboratorio 2: catálogo indexado y confiable

**Estudiante:** _Completar con el nombre antes de entregar._

Este proyecto implementa un catálogo musical binario de registros de longitud
variable, un índice primario `label_id -> offset`, un índice secundario invertido
`composer -> lista<label_id>` y una auditoría de consistencia. Cada lectura valida
el encabezado, los límites, el CRC-32 y la estructura lógica del payload antes de
entregar un registro.

También se implementó el bono `intersect_sorted` mediante dos punteros.

## Compilar

```bash
cmake -S . -B build
cmake --build build
```

## Ejecutar pruebas

```bash
ctest --test-dir build --output-on-failure
```

Con Visual Studio en Windows, indique además la configuración:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

O directamente (en un generador de una sola configuración):

```bash
./build/catalog_tests
```

Con Visual Studio, el ejecutable equivalente es
`build/Debug/catalog_tests.exe`.

Para ejecutar un ejercicio específico:

```bash
./build/catalog_tests --test-case="E04*"
```

## Generar datos de ejemplo

```bash
./build/catalog_generate data/catalog.psv data/catalog.bin
```

## Usar la aplicación

```bash
./build/lab2_catalog build data/catalog.bin data/catalog.idx
./build/lab2_catalog find data/catalog.bin data/catalog.idx DG18807
./build/lab2_catalog composer data/catalog.bin data/catalog.idx BEETHOVEN
./build/lab2_catalog verify data/catalog.bin data/catalog.idx
```

## Simular corrupción

```bash
./build/catalog_corrupt flip data/catalog.bin data/catalog.corrupt 20
./build/lab2_catalog verify data/catalog.corrupt data/catalog.idx

./build/catalog_corrupt truncate data/catalog.bin data/catalog.truncated 3
./build/lab2_catalog verify data/catalog.truncated data/catalog.idx
```

## Archivos que ya están completos

- `src/binary_io.cpp`: I/O little-endian y utilidades de stream.
- `src/crc32.cpp`: CRC-32/ISO-HDLC.
- `src/catalog_codec.cpp`: codificación y decodificación del payload.
- `src/index_io.cpp`: persistencia del índice primario.
- `src/main.cpp`: interfaz de línea de comandos.
- `tools/`: generación y corrupción controlada de datos.

## Complejidad

- `read_record_at`: O(p) tiempo y O(p) memoria, donde `p` es el tamaño del payload.
- Construcción del índice primario: O(n log n) tiempo por el ordenamiento y O(n)
  memoria, además del costo de leer los payloads.
- Búsqueda primaria: O(log n) en memoria, seguida de un `seek` y la lectura validada
  de un único registro.
- Construcción del índice por compositor: O(n log n) tiempo y O(n) memoria.
- Búsqueda por compositor: O(log c), donde `c` es la cantidad de compositores; el
  resultado es un `span` sobre la lista ya almacenada.
- Auditoría: O(n log n) por las copias ordenadas usadas para detectar duplicados,
  más O(n) lecturas validadas.
- Intersección opcional: O(n + m) tiempo y memoria proporcional al resultado.

## Integridad física y consistencia lógica

La **integridad física** comprueba que los bytes estén completos y sin alteraciones;
por ejemplo, detecta payloads truncados o un CRC-32 incorrecto. La **consistencia
lógica** comprueba que esos bytes y sus referencias tengan sentido para el catálogo;
por ejemplo, que el magic y la versión sean válidos, que las claves sean únicas y
ordenadas, y que cada clave del índice coincida con el registro al que apunta. Un CRC
válido no garantiza por sí solo que el índice apunte al registro correcto.

## Pruebas adicionales

`tests/student_tests.cpp` incluye casos para:

- archivo vacío;
- header truncado y la invariante de no entregar un registro inválido;
- referencia secundaria cuya clave no coincide;
- duplicados presentes en un índice desordenado;
- intersección ordenada con entradas duplicadas.

