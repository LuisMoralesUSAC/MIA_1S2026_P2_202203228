# Manual Técnico - Sistema de Archivos EXT2/EXT3

## Tabla de Contenidos

1. [Introducción](#introducción)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Despliegue en AWS](#despliegue-en-aws)
4. [Estructuras de Datos](#estructuras-de-datos)
5. [Comandos Implementados](#comandos-implementados)
6. [Módulos del Sistema](#módulos-del-sistema)
7. [Flujo de Ejecución](#flujo-de-ejecución)

---

## Introducción

Este proyecto implementa un sistema de gestión de discos virtuales y un sistema de archivos tipo EXT2/EXT3 en C++, expuesto mediante una API REST e interfaz gráfica web. El sistema permite:

- Creación y administración de discos virtuales (.mia)
- Particionamiento de discos (primarias, extendidas, lógicas) con soporte de ADD y DELETE
- Formateo con sistema de archivos EXT2 y EXT3
- Gestión de usuarios y grupos
- Operaciones sobre archivos y directorios (remove, rename, copy, move, find, chown, chmod)
- Journaling de operaciones en particiones EXT3
- Simulación de pérdida de sistema de archivos (loss)
- Visualizador gráfico de discos, particiones y archivos en el frontend
- Generación de reportes gráficos mediante Graphviz
- Despliegue en la nube mediante AWS (EC2 + S3)

### Tecnologías Utilizadas

- **Backend:** C++ 17
- **Frontend:** React (Create React App)
- **Servidor intermediario:** Node.js con Express
- **Reportes:** Graphviz (DOT)
- **Compilador:** g++ (GCC)
- **Nube:** AWS EC2 (backend) y AWS S3 (frontend)

---

## Arquitectura del Sistema

El sistema está compuesto por cuatro capas principales:

```
┌─────────────────────────────────────────┐
│         Interfaz Web (React)            │
│   AWS S3 - Sitio Web Estático           │
└──────────────────┬──────────────────────┘
                   │ HTTP (axios)
                   ▼
┌─────────────────────────────────────────┐
│        Servidor Web (Node.js)           │
│    Express + Body-Parser — Puerto 4000  │
│         AWS EC2 (Ubuntu Linux)          │
└──────────────────┬──────────────────────┘
                   │ spawn()
                   ▼
┌─────────────────────────────────────────┐
│        Motor de Comandos (C++)          │
│        Ejecutable: disk_manager         │
│         AWS EC2 (Ubuntu Linux)          │
└─────────────────────────────────────────┘
```

### Componentes

#### 1. Frontend React (`/frontend/`)

- **`src/App.js`**: Componente raíz. Gestiona el estado global de sesión, página activa (principal / login / visualizador) y disco/partición seleccionados.
- **`src/pages/PaginaPrincipal.jsx`**: Página de inicio con la terminal de comandos y botones de sesión/visualizador.
- **`src/pages/PaginaLogin.jsx`**: Formulario gráfico de inicio de sesión (ID de partición, usuario, contraseña).
- **`src/components/Terminal.jsx`**: Área de entrada y salida de comandos. Permite cargar archivos `.smia` y ejecutar scripts completos.
- **`src/components/SelectorDiscos.jsx`**: Muestra los discos registrados (con particiones montadas) y permite seleccionarlos.
- **`src/components/SelectorParticion.jsx`**: Muestra las particiones de un disco con información detallada (tamaño, tipo, fit, estado).
- **`src/components/ExplorarArchivos.jsx`**: Navegador de archivos en modo lectura. Permite explorar carpetas y visualizar contenido de archivos. Incluye breadcrumb y acceso al journal.
- **`src/components/VisualizadorJournal.jsx`**: Modal con tabla paginada y filtrable de las transacciones del journal de una partición EXT3.
- **`src/services/api.js`**: Capa de comunicación con el servidor Node.js mediante axios.

#### 2. Servidor Web (`/server/server.js`)

- Recibe peticiones del frontend vía API REST.
- Mantiene en memoria el registro de discos montados (`registeredDisks`) y el orden de montaje (`mountOrder`) para poder reconstruir el estado del proceso C++ en cada invocación.
- Mantiene la sesión activa del servidor (`sesionActivaServidor`) para inyectarla automáticamente en cada ejecución de script.
- Ejecuta el binario C++ (`disk_manager`) mediante `spawn()`, enviando comandos por stdin y leyendo resultados por stdout.
- Endpoints disponibles:

| Endpoint | Método | Descripción |
|---|---|---|
| `/api/execute-script` | POST | Ejecutar uno o varios comandos |
| `/api/login` | POST | Iniciar sesión gráfico |
| `/api/logout` | POST | Cerrar sesión |
| `/api/disks` | GET | Listar discos registrados |
| `/api/disks/partitions` | POST | Obtener info de particiones de un disco |
| `/api/filesystem` | POST | Listar directorio |
| `/api/filesystem/file` | POST | Leer contenido de archivo |
| `/api/journaling` | GET | Ver journal de una partición EXT3 |
| `/api/health` | GET | Estado del servidor |

#### 3. Backend C++ (`/backend/`)

- `main.cpp`: Punto de entrada. Parser de comandos case-insensitive con soporte para parámetros en cualquier orden y comillas simples/dobles.
- `structures.h`: Definición de todas las estructuras de datos.
- Un archivo `.h` por cada comando o módulo funcional.

---

## Despliegue en AWS

### Arquitectura en la nube

```
                         AWS
          ┌──────────────────────────────┐
          │                              │
  Usuario │  S3 Bucket (Frontend React)  │
  ──────► │  URL pública del sitio web   │
          │                              │
          │        ┌────────────────┐    │
          │        │   EC2 Ubuntu   │    │
          │        │ Node.js :4000  │    │
          │        │ disk_manager   │    │
          │        └────────────────┘    │
          └──────────────────────────────┘
```

### EC2 (Backend)

1. **Instancia:** Ubuntu Server 22.04 LTS (t2.micro o superior).
2. **Security Groups:** Puertos abiertos: 22 (SSH), 4000 (API Node.js).
3. **Instalación de dependencias:**
   ```bash
   sudo apt update
   sudo apt install -y g++ nodejs npm graphviz
   ```
4. **Compilación del backend:**
   ```bash
   cd backend
   g++ -std=c++17 -Wall -I. main.cpp -o disk_manager
   chmod +x disk_manager
   ```
5. **Inicio del servidor Node.js:**
   ```bash
   cd server
   npm install
   npm start
   ```

### S3 (Frontend)

1. **Build de producción:**
   ```bash
   cd frontend
   npm start
   ```
2. **Crear bucket S3** con nombre único, región deseada.
3. **Habilitar hospedaje de sitio web estático** en las propiedades del bucket.
4. **Subir la carpeta `build/`** al bucket.
5. **Configurar política de bucket** para acceso público de lectura:
   ```json
   {
     "Version": "2012-10-17",
     "Statement": [{
       "Effect": "Allow",
       "Principal": "*",
       "Action": "s3:GetObject",
       "Resource": "arn:aws:s3:::NOMBRE-DEL-BUCKET/*"
     }]
   }
   ```
6. El frontend queda accesible en la URL de endpoint S3.

### Configuración CORS en el servidor

Para que el frontend en S3 pueda comunicarse con el backend en EC2, el servidor Node.js incluye el middleware `cors`:

```javascript
app.use(cors()); // Permite peticiones desde cualquier origen
```

---

## Estructuras de Datos

Definidas en `structures.h`.

### 1. Master Boot Record (MBR)

```cpp
struct MBR {
    int mbr_size;                // Tamaño total del disco en bytes
    time_t mbr_creation_date;    // Fecha de creación
    int mbr_disk_signature;      // Firma única del disco
    char disk_fit;               // Ajuste: 'B', 'F', 'W'
    Partition mbr_partitions[4]; // Array de 4 particiones
};
```

### 2. Partition

```cpp
struct Partition {
    char part_status;      // '0' = inactiva, '1' = activa
    char part_type;        // 'P' = Primaria, 'E' = Extendida
    char part_fit;         // 'B' = Best, 'F' = First, 'W' = Worst
    int part_start;        // Byte de inicio
    int part_size;         // Tamaño en bytes
    char part_name[16];    // Nombre de la partición
};
```

### 3. Extended Boot Record (EBR)

```cpp
struct EBR {
    char part_status;      // Estado de la partición lógica
    char part_fit;         // Ajuste de la partición
    int part_start;        // Byte de inicio
    int part_size;         // Tamaño en bytes
    int part_next;         // Byte del siguiente EBR (-1 si no hay)
    char part_name[16];    // Nombre de la partición
};
```

### 4. Superblock

```cpp
struct Superblock {
    int s_filesystem_type;    // 2 = EXT2, 3 = EXT3
    int s_inodes_count;       // Total de inodos
    int s_blocks_count;       // Total de bloques
    int s_free_blocks_count;  // Bloques libres
    int s_free_inodes_count;  // Inodos libres
    time_t s_mtime;           // Última fecha de montaje
    time_t s_umtime;          // Última fecha de desmontaje
    int s_mnt_count;          // Contador de montajes
    int s_magic;              // 0xEF53
    int s_inode_size;         // Tamaño del inodo
    int s_block_size;         // Tamaño del bloque (64 bytes)
    int s_first_ino;          // Primer inodo libre
    int s_first_blo;          // Primer bloque libre
    int s_bm_inode_start;     // Inicio bitmap de inodos
    int s_bm_block_start;     // Inicio bitmap de bloques
    int s_inode_start;        // Inicio tabla de inodos
    int s_block_start;        // Inicio de bloques
    int s_journal_start;      // Inicio del área de journaling (EXT3)
};
```

### 5. Inode

```cpp
struct Inode {
    int i_uid;           // UID del propietario
    int i_gid;           // GID del grupo
    int i_size;          // Tamaño en bytes
    time_t i_atime;      // Último acceso
    time_t i_ctime;      // Fecha de creación
    time_t i_mtime;      // Última modificación
    int i_block[15];     // Apuntadores a bloques (12 directos + 3 indirectos)
    char i_type;         // '0' = archivo, '1' = carpeta
    int i_perm;          // Permisos en formato octal (ej: 664)
};
```

### 6. FolderBlock (Bloque de Carpeta)

```cpp
struct Content {
    char b_name[12];     // Nombre del archivo/carpeta
    int b_inodo;         // Número de inodo (-1 si vacío)
};

struct FolderBlock {
    Content b_content[4]; // 4 entradas por bloque
};
```

### 7. FileBlock (Bloque de Archivo)

```cpp
struct FileBlock {
    char b_content[64];  // Contenido del archivo (64 bytes)
};
```

### 8. PointerBlock (Bloque de Apuntadores)

```cpp
struct PointerBlock {
    int b_pointers[16];  // 16 apuntadores a bloques (-1 si vacío)
};
```

### 9. Information (Entrada de Journal)

```cpp
struct Information {
    char i_operation[10];  // Tipo de operación (mkdir, mkfile, remove...)
    char i_path[32];       // Ruta donde se realizó la operación
    char i_content[64];    // Contenido relevante (si es archivo)
    float i_date;          // Fecha en segundos desde epoch (time_t como float)
};
```

### 10. Journal

```cpp
struct Journal {
    int j_count;               // Contador total de operaciones registradas
    Information j_content[50]; // Hasta 50 entradas (rotación circular)
};
```

### Layout de partición EXT2

```
┌─────────────────┐ ← Inicio partición
│   Superblock    │
├─────────────────┤
│ Bitmap Inodos   │ (n bytes)
├─────────────────┤
│ Bitmap Bloques  │ (3n bytes)
├─────────────────┤
│ Tabla Inodos    │ (n × sizeof(Inode))
├─────────────────┤
│    Bloques      │ (3n × 64 bytes)
└─────────────────┘
```

Donde `n` se calcula como:
```
n = floor((tamaño_partición - sizeof(Superblock)) / (4 + sizeof(Inode) + 3 × 64))
```

### Layout de partición EXT3

```
┌─────────────────┐ ← Inicio partición
│   Superblock    │
├─────────────────┤
│   Journal       │ (sizeof(Journal) bytes, ubicado en bloque 2+)
├─────────────────┤
│ Bitmap Inodos   │ (n bytes)
├─────────────────┤
│ Bitmap Bloques  │ (3n bytes)
├─────────────────┤
│ Tabla Inodos    │ (n × sizeof(Inode))
├─────────────────┤
│    Bloques      │ (3n × 64 bytes)
└─────────────────┘
```

Donde `n` se calcula como:
```
tamaño_partición = sizeof(Superblock) + n×sizeof(Journal) + n + 3n + n×sizeof(Inode) + 3n×sizeof(block)
n = floor(resultado)
```

El valor de Journaling se maneja con una constante de 50 entradas.

---

## Comandos Implementados

### Gestión de Discos

#### MKDISK
**Sintaxis:**
```bash
mkdisk -size=N -unit=[k|m] -path=ruta
```

**Parámetros:**
- `-size`: Tamaño del disco (obligatorio, mayor a 0)
- `-unit`: Unidad (`k`=KB, `m`=MB, por defecto `m`)
- `-path`: Ruta del archivo .mia (obligatorio, crea directorios padre si no existen)

**Archivo:** `mkdisk.h`

#### RMDISK
**Sintaxis:**
```bash
rmdisk -path=ruta
```
**Archivo:** `rmdisk.h`

---

### Gestión de Particiones

#### FDISK
**Sintaxis:**
```bash
# Crear partición
fdisk -size=N -unit=[b|k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre

# Eliminar partición (fast o full)
fdisk -delete=[fast|full] -name=nombre -path=ruta

# Agregar/quitar espacio
fdisk -add=N -unit=[b|k|m] -path=ruta -name=nombre
```

**Parámetros de eliminación:**
- `-delete=fast`: Marca el espacio como vacío en la tabla de particiones.
- `-delete=full`: Además, rellena el espacio con `\0`. Si es extendida, elimina también todas las lógicas.

**Parámetro de redimensión:**
- `-add`: Valor positivo para ampliar, negativo para reducir. Verifica espacio disponible adyacente al ampliar.

**Archivo:** `fdisk.h`

#### MOUNT
**Sintaxis:**
```bash
mount -path=ruta -name=nombre
```

**Generación de IDs — formato `DDNL`:**
- `DD`: Últimos 2 dígitos del carnet (28)
- `N`: Número de partición en ese disco
- `L`: Letra del disco en mayúscula (A, B, C...)

**Ejemplos:** Disco1/Part1 → `281A` | Disco1/Part2 → `282A` | Disco2/Part1 → `281B`

**Archivo:** `mount.h`

#### UNMOUNT
**Sintaxis:**
```bash
unmount -id=ID
```

Desmonta la partición, eliminándola del mapa de particiones montadas. No puede desmontar si hay sesión activa en esa partición. Resetea el correlativo de partición para ese disco si no quedan más particiones montadas.

**Archivo:** `unmount.h`

---

### Sistema de Archivos

#### MKFS
**Sintaxis:**
```bash
mkfs -id=ID [-type=full] [-fs=2fs|3fs]
```

**Parámetros:**
- `-id`: ID de partición montada (obligatorio)
- `-type`: Tipo de formateo (`full` es el único valor, por defecto `full`)
- `-fs`: Sistema de archivos (`2fs`=EXT2, `3fs`=EXT3, por defecto EXT2)

**Inicialización EXT2:**
1. Crea Superblock (`s_filesystem_type = 2`)
2. Inicializa bitmaps en `'0'`
3. Crea inodo 0 (raíz `/`, carpeta)
4. Crea inodo 1 (archivo `/users.txt` con contenido `1,G,root\n1,U,root,root,123\n`)
5. Marca inodos y bloques 0 y 1 como usados

**Inicialización adicional EXT3:**
- Reserva bloques para el `Journal` (desde bloque 2)
- Escribe estructura `Journal` vacía con `j_count = 0`
- Actualiza `s_journal_start` en el Superblock

**Archivo:** `mkfs.h`

---

### Gestión de Usuarios

#### LOGIN
```bash
login -user=usuario -pass=contraseña -id=ID
```
Lee `/users.txt` del disco, valida credenciales, establece sesión con UID, GID e `isRoot`.

#### LOGOUT
```bash
logout
```
Cierra la sesión activa.

#### MKGRP / RMGRP
```bash
mkgrp -name=nombre
rmgrp -name=nombre
```
Solo root puede ejecutarlos. `rmgrp` no elimina físicamente la línea, sino que pone ID `0` para marcarla como eliminada.

#### MKUSR / RMUSR
```bash
mkusr -user=usuario -pass=contraseña -grp=grupo
rmusr -user=usuario
```
Solo root. Nombres máx. 10 caracteres. `rmusr` marca con ID `0`.

#### CHGRP
```bash
chgrp -user=usuario -grp=grupo
```
Solo root. Cambia el grupo al que pertenece un usuario en `/users.txt`.

**Archivos:** `login.h`, `logout.h`, `mkgrp.h`, `rmgrp.h`, `mkusr.h`, `rmusr.h`, `chgrp.h`

---

### Gestión de Archivos y Directorios

#### MKDIR
```bash
mkdir [-p] -path=ruta
```
Con `-p` crea todos los directorios intermedios que no existan. Sin `-p`, el directorio padre debe existir.

#### MKFILE
```bash
mkfile [-r] -path=ruta [-size=N] [-cont=archivo_real]
```
- Con `-size`: Genera contenido con el patrón `0123456789` repetido.
- Con `-cont`: Copia el contenido de un archivo del sistema real.
- Con `-r`: Crea carpetas padre faltantes (equivalente a `-p` de mkdir).
- Soporta bloques directos (0-11), indirecto simple (bloque 12), doble (13) y triple (14).

#### CAT
```bash
cat -file1=ruta [-file2=ruta] ...
```
Lee y muestra el contenido de uno o más archivos. Valida permisos de lectura. Soporta apuntadores directos, simples, dobles y triples.

#### REMOVE
```bash
remove -path=ruta
```
Elimina archivo o carpeta recursivamente. Primero verifica permisos de escritura en **todos** los elementos de la jerarquía; si alguno falla, no elimina nada. Libera inodos y bloques en los bitmaps.

#### RENAME
```bash
rename -path=ruta -name=nuevo_nombre
```
Cambia el nombre de un archivo o carpeta. Requiere permiso de escritura. Verifica que el nuevo nombre no exista ya en el mismo directorio.

#### COPY
```bash
copy -path=origen -destino=destino
```
Copia archivo o carpeta recursivamente. Respeta permisos de lectura en origen y escritura en destino. Los archivos sin permiso de lectura se omiten silenciosamente (no se copia solo ese elemento, el resto continúa).

#### MOVE
```bash
move -path=origen -destino=destino
```
Mueve archivo o carpeta. Solo requiere permiso de escritura sobre el origen. Si origen y destino están en la misma partición, actualiza referencias sin copiar datos: elimina la entrada del padre origen y la agrega al padre destino. Actualiza la entrada `..` dentro del inodo movido si es carpeta.

#### FIND
```bash
find -path=directorio -name=patron
```
Búsqueda recursiva por nombre. Soporta wildcards:
- `?` — exactamente un carácter
- `*` — uno o más caracteres

Requiere permiso de lectura en los directorios recorridos.

#### CHOWN
```bash
chown [-r] -path=ruta -usuario=nombre
```
Cambia el propietario (UID). Root puede hacerlo en cualquier archivo; otros usuarios solo en sus propios archivos. Con `-r` aplica recursivamente a toda la jerarquía.

#### CHMOD
```bash
chmod [-r] -path=ruta -ugo=NNN
```
Cambia permisos en formato octal de 3 dígitos (0-7 cada uno, representando U/G/O). Con `-r` aplica recursivamente. Cada dígito en binario representa: bit 2 = lectura, bit 1 = escritura, bit 0 = ejecución.

**Archivos:** `mkdir.h`, `mkfile.h`, `cat.h`, `remove.h`, `rename.h`, `copy.h`, `move.h`, `find.h`, `chown.h`, `chmod.h`

---

### Journaling (EXT3)

#### JOURNALING
```bash
journaling -id=ID
```
Muestra en la salida estándar (y en la interfaz gráfica) todas las transacciones registradas en el Journal de la partición EXT3 indicada. El Journal usa rotación circular con máximo 50 entradas.

Las operaciones que registran entradas son: `mkdir`, `mkfile`, `remove`, `rename`, `copy`, `move`, `chown`, `chmod`, `mkusr`, `rmusr`, `mkgrp`, `chgrp`.

**Archivo:** `journal_helpers.h`, `journaling.h`

#### LOSS
```bash
loss -id=ID
```
Simula pérdida del sistema de archivos EXT3. Limpia con `\0` las siguientes áreas:
- Bitmap de inodos
- Bitmap de bloques
- Área de inodos
- Área de bloques (preservando el Journal y el Superblock)

Solo aplica a particiones EXT3.

**Archivo:** `loss.h`

---

### Reportes

#### REP
```bash
rep -id=ID -path=ruta_salida -name=tipo [-path_file_ls=ruta]
```

| Tipo | Descripción | Formato |
|---|---|---|
| `mbr` | Estructura del MBR | JPG/PNG |
| `disk` | Ocupación del disco | JPG/PNG |
| `inode` | Tabla de inodos usados | JPG/PNG |
| `block` | Bloques utilizados | JPG/PNG |
| `bm_inode` | Bitmap de inodos | TXT |
| `bm_block` | Bitmap de bloques | TXT |
| `tree` | Árbol del sistema de archivos | PNG |
| `sb` | Superbloque | JPG/PNG |
| `file` | Contenido de archivo | JPG/PNG |
| `ls` | Listado de directorio | JPG/PNG o TXT |

El reporte `ls` en formato `.txt` es utilizado internamente por el servidor Node.js para alimentar el visualizador gráfico de archivos del frontend.

**Archivo:** `rep.h`

---

## Módulos del Sistema

### 1. Parser de Comandos (`main.cpp`)

- `parseParameter(commandLine, paramName)`: Extrae el valor de un parámetro, soportando comillas simples, dobles y parámetros en cualquier orden.
- `hasFlag(commandLine, flagName)`: Detecta flags booleanos como `-p` o `-r`.
- `executeCommand(commandLine)`: Enruta el comando al manejador correspondiente.
- Detecta automáticamente mounts, logins y logouts en scripts para actualizar el estado del servidor Node.js.

### 2. Gestión de Sesiones (`session.h`)

```cpp
struct Session {
    std::string currentUser;  // Usuario actual
    std::string currentID;    // ID de partición montada
    int uid;                  // UID del usuario
    int gid;                  // GID del grupo
    bool isRoot;              // Si es usuario root (uid==1 && gid==1)
    bool active;              // Si hay sesión activa
};
```

### 3. Sistema de Montaje (`mount.h`)

```cpp
struct MountedPartition {
    std::string path;   // Ruta del disco
    std::string name;   // Nombre de partición
    std::string id;     // ID de montaje (ej: 281A)
    char type;          // P, E, L
    int start;          // Byte de inicio
    int size;           // Tamaño
};

// Mapa global de particiones montadas
static std::map<std::string, MountedPartition> mountedPartitions;
```

### 4. Helpers del Sistema de Archivos (`filesystem_helpers.h`)

Funciones principales agrupadas por categoría:

**Bitmaps:**
- `encontrarInodoLibre()`, `encontrarBloqueLibre()`
- `marcarInodoUsado()`, `marcarBloqueUsado()`

**Inodos:**
- `leerInodo()`, `escribirInodo()`

**Bloques:**
- `leerBloqueCarpeta()`, `escribirBloqueCarpeta()`
- `leerBloqueArchivo()`, `escribirBloqueArchivo()`
- `leerBloqueApuntadores()`, `escribirBloqueApuntadores()`

**Navegación:**
- `dividirRuta()`: Divide una ruta en componentes.
- `navegarRuta()`: Navega el árbol de inodos hasta la ruta indicada.
- `obtenerNombreArchivo()`, `obtenerRutaPadre()`

**Permisos:**
- `verificarPermiso(inodo, uid, gid, esRoot, tipo)`: Evalúa permisos UGO en formato octal. Root siempre tiene acceso.

**Utilidades:**
- `crearCarpeta()`: Crea un directorio con inodo y bloque inicializados (entradas `.` y `..`).
- `agregarEntradaEnBloque()`: Agrega una entrada en un bloque de carpeta existente.
- `actualizarSuperblock()`

### 5. Journal Helpers (`journal_helpers.h`)

- `esExt3(id)`: Verifica si la partición es EXT3.
- `leerJournal()`, `escribirJournal()`: Lectura/escritura de la estructura `Journal` en disco.
- `registrar(id, operacion, ruta, contenido)`: Agrega una entrada al journal usando rotación circular (`idx = j_count % 50`). Solo actúa en particiones EXT3.
- `mostrarJournal(id)`: Formatea el journal como tabla de texto para la salida del comando `journaling`.
- `formatearFecha(float)`: Convierte el campo `i_date` (segundos epoch) a string legible.

---

## Flujo de Ejecución

### Ejemplo: Crear archivo con MKFILE

```
1. Usuario escribe en la terminal web:
   mkfile -path=/home/docs/nota.txt -size=100

2. Frontend (Terminal.jsx) envía el script al servidor via POST /api/execute-script

3. Servidor Node.js (server.js):
   ├─ Prepend: mount -path=... -name=... (reconstituye estado)
   ├─ Prepend: login -user=... -pass=... -id=...
   └─ Ejecuta disk_manager con todos los comandos via spawn()

4. Parser (main.cpp):
   ├─ Detecta comando "mkfile"
   ├─ Extrae parámetros: path=/home/docs/nota.txt, size=100
   └─ Llama comandoMkfile()

5. comandoMkfile() (mkfile.h):
   ├─ Valida sesión activa
   ├─ Obtiene partición montada del ID de sesión
   ├─ Abre archivo del disco
   ├─ Lee Superblock
   ├─ Divide ruta: padre="/home/docs", nombre="nota.txt"
   ├─ Navega a /home/docs (navegarRuta)
   ├─ Valida permisos de escritura en carpeta padre
   ├─ Busca inodo libre (encontrarInodoLibre)
   ├─ Crea Inode (tipo '0', size=100, uid=sesión.uid)
   ├─ Genera contenido: "0123456789..." (100 bytes)
   ├─ Escribe bloques:
   │  ├─ Bloque directo 0: bytes 0-63
   │  └─ Bloque directo 1: bytes 64-99
   ├─ Marca inodos/bloques usados en bitmaps
   ├─ Agrega entrada en bloque de carpeta padre
   ├─ Actualiza Superblock (free_inodes--, free_blocks-=2)
   ├─ Si partición es EXT3: llama JournalHelpers::registrar()
   └─ Retorna "Archivo creado exitosamente: /home/docs/nota.txt (100 bytes)"

6. Servidor retorna resultado al frontend (JSON)

7. Terminal.jsx muestra en la consola la salida del comando
```
