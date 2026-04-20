#ifndef RENAME_H
#define RENAME_H

#include <string>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

inline std::string comandoRename(const std::string& ruta, const std::string& nuevo_nombre) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }
    
    if (ruta.empty() || ruta[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }

    if (ruta == "/") {
        return "Error: No se puede renombrar el directorio raíz";
    }

    if (nuevo_nombre.empty()) {
        return "Error: El nuevo nombre no puede estar vacío";
    }

    if (nuevo_nombre.length() > 12) {
        return "Error: El nuevo nombre no puede exceder 12 caracteres";
    }

    // Validar que el nuevo nombre no contenga /
    if (nuevo_nombre.find('/') != std::string::npos) {
        return "Error: El nuevo nombre no puede contener '/'";
    }
    
    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;

    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }

    FILE* archivo = fopen(particion.path.c_str(), "r+b");
    if (!archivo) {
        return "Error: No se pudo abrir el disco";
    }

    Superblock super;
    fseek(archivo, particion.start, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);

    // Navegar a la ruta objetivo
    ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
    if (!nav.exito) {
        fclose(archivo);
        return "Error: No se encontró '" + ruta + "'";
    }

    // Verificar permiso de escritura sobre el archivo/carpeta
    Inode inodo_objetivo = leerInodo(archivo, super, nav.inodo_actual);
    if (!verificarPermiso(inodo_objetivo,
                          SessionManager::currentSession.uid,
                          SessionManager::currentSession.gid,
                          SessionManager::currentSession.isRoot,
                          PERMISO_ESCRITURA)) {
        fclose(archivo);
        return "Error: No tiene permisos de escritura sobre '" + ruta + "'";
    }

    // Verificar que el nuevo nombre no exista en el directorio padre
    std::string ruta_padre = obtenerRutaPadre(ruta);
    ResultadoNavegacion nav_padre = navegarRuta(archivo, super, ruta_padre, false);
    if (!nav_padre.exito) {
        fclose(archivo);
        return "Error: No se pudo acceder al directorio padre";
    }

    Inode inodo_padre = leerInodo(archivo, super, nav_padre.inodo_actual);
    for (int i = 0; i < 12 && inodo_padre.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_padre.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;

            char buf[13] = {0};
            strncpy(buf, fb.b_content[j].b_name, 12);
            std::string nombre_existente(buf);

            if (nombre_existente == nuevo_nombre) {
                fclose(archivo);
                return "Error: Ya existe un archivo o carpeta con el nombre '"
                       + nuevo_nombre + "' en ese directorio";
            }
        }
    }

    // Actualizar el nombre en el bloque del padre
    bool renombrado = false;
    std::string nombre_actual = obtenerNombreArchivo(ruta);

    inodo_padre = leerInodo(archivo, super, nav_padre.inodo_actual);
    for (int i = 0; i < 12 && inodo_padre.i_block[i] != -1 && !renombrado; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_padre.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo != nav.inodo_actual) continue;

            memset(fb.b_content[j].b_name, 0, 12);
            strncpy(fb.b_content[j].b_name, nuevo_nombre.c_str(), 12);
            escribirBloqueCarpeta(archivo, super, inodo_padre.i_block[i], fb);
            renombrado = true;
            break;
        }
    }

    if (!renombrado) {
        fclose(archivo);
        return "Error: No se pudo encontrar la entrada en el directorio padre";
    }

    // Actualizar tiempo de modificación del inodo
    inodo_objetivo.i_mtime = std::time(nullptr);
    escribirInodo(archivo, super, nav.inodo_actual, inodo_objetivo);

    fclose(archivo);
    return "'" + nombre_actual + "' renombrado a '" + nuevo_nombre + "' exitosamente";
}

#endif