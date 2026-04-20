#ifndef FIND_H
#define FIND_H

#include <string>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

// Función de matching con wildcards: ? = un carácter, * = cero o más caracteres
inline bool matchWildcard(const std::string& patron, const std::string& nombre) {
    int p = 0, n = 0;
    int starPos = -1, matchPos = 0;

    while (n < (int)nombre.size()) {
        if (p < (int)patron.size() && (patron[p] == '?' || patron[p] == nombre[n])) {
            p++;
            n++;
        } else if (p < (int)patron.size() && patron[p] == '*') {
            // '*' requiere al menos un carácter
            if (n >= (int)nombre.size()) return false;
            starPos = p;
            matchPos = n + 1;
            p++;
            n = matchPos;
        } else if (starPos != -1) {
            p = starPos + 1;
            matchPos++;
            n = matchPos;
        } else {
            return false;
        }
    }

    while (p < (int)patron.size() && patron[p] == '*') p++;
    return p == (int)patron.size();
}

// Búsqueda recursiva en el árbol de directorios
inline void buscarRecursivo(FILE* archivo, const Superblock& super,
                             int inodo_num, const std::string& ruta_actual,
                             const std::string& patron,
                             int uid, int gid, bool es_root,
                             std::vector<std::string>& resultados) {

    Inode inodo = leerInodo(archivo, super, inodo_num);

    // Verificar permiso de lectura/ejecución sobre el directorio
    if (!verificarPermiso(inodo, uid, gid, es_root, PERMISO_LECTURA)) {
        return;
    }

    if (inodo.i_type != '1') return; // Solo procesar carpetas

    for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo.i_block[i]);

        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;

            char buf[13] = {0};
            strncpy(buf, fb.b_content[j].b_name, 12);
            std::string nombre(buf);

            if (nombre == "." || nombre == "..") continue;

            std::string ruta_hijo = (ruta_actual == "/")
                ? "/" + nombre
                : ruta_actual + "/" + nombre;

            // Verificar si el nombre coincide con el patrón
            if (matchWildcard(patron, nombre)) {
                resultados.push_back(ruta_hijo);
            }

            // Si es carpeta, buscar recursivamente
            int hijo_inodo = fb.b_content[j].b_inodo;
            if (hijo_inodo < 0 || hijo_inodo >= super.s_inodes_count) continue;

            Inode inodo_hijo = leerInodo(archivo, super, hijo_inodo);
            if (inodo_hijo.i_type == '1') {
                buscarRecursivo(archivo, super, hijo_inodo, ruta_hijo,
                                patron, uid, gid, es_root, resultados);
            }
        }
    }
}

inline std::string comandoFind(const std::string& ruta_inicio,
                                const std::string& patron) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }

    if (ruta_inicio.empty() || ruta_inicio[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }

    if (patron.empty()) {
        return "Error: Debe especificar un nombre o patrón con -name";
    }

    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;

    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }

    FILE* archivo = fopen(particion.path.c_str(), "rb");
    if (!archivo) {
        return "Error: No se pudo abrir el disco";
    }

    Superblock super;
    fseek(archivo, particion.start, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);

    // Navegar al directorio de inicio
    ResultadoNavegacion nav = navegarRuta(archivo, super, ruta_inicio, false);
    if (!nav.exito) {
        fclose(archivo);
        return "Error: No se encontró la ruta '" + ruta_inicio + "'";
    }

    Inode inodo_inicio = leerInodo(archivo, super, nav.inodo_actual);
    if (inodo_inicio.i_type != '1') {
        fclose(archivo);
        return "Error: '" + ruta_inicio + "' no es un directorio";
    }

    std::vector<std::string> resultados;

    buscarRecursivo(archivo, super, nav.inodo_actual, ruta_inicio,
                    patron,
                    SessionManager::currentSession.uid,
                    SessionManager::currentSession.gid,
                    SessionManager::currentSession.isRoot,
                    resultados);

    fclose(archivo);

    if (resultados.empty()) {
        return "No se encontraron archivos con el patrón '" + patron +
               "' en '" + ruta_inicio + "'";
    }

    std::string salida = "Resultados de búsqueda:\n";
    for (const std::string& r : resultados) {
        salida += "  " + r;
    }

    return salida;
}

#endif