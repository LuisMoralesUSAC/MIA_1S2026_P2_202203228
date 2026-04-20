#ifndef CHMOD_H
#define CHMOD_H

#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

inline bool validarUgo(const std::string& ugo) {
    if (ugo.length() != 3) return false;
    for (char c : ugo) {
        if (c < '0' || c > '7') return false;
    }
    return true;
}

inline int ugoAPermisos(const std::string& ugo) {
    return ((ugo[0] - '0') * 100) + ((ugo[1] - '0') * 10) + (ugo[2] - '0');
}

inline void chmodRecursivoDir(FILE* archivo, Superblock& super,
                               int inodo_num, int nuevos_permisos,
                               int uid_actual, bool es_root) {
    Inode inodo = leerInodo(archivo, super, inodo_num);

    // Solo aplica si es root o es propietario
    if (es_root || inodo.i_uid == uid_actual) {
        inodo.i_perm  = nuevos_permisos;
        inodo.i_mtime = std::time(nullptr);
        escribirInodo(archivo, super, inodo_num, inodo);
    }

    if (inodo.i_type != '1') return;

    for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            char buf[13] = {0};
            strncpy(buf, fb.b_content[j].b_name, 12);
            std::string nombre(buf);
            if (nombre == "." || nombre == "..") continue;

            chmodRecursivoDir(archivo, super, fb.b_content[j].b_inodo,
                              nuevos_permisos, uid_actual, es_root);
        }
    }
}

inline std::string comandoChmod(const std::string& ruta,
                                 const std::string& ugo,
                                 bool recursivo) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }

    if (ruta.empty() || ruta[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }

    if (!validarUgo(ugo)) {
        return "Error: El valor de -ugo debe ser tres dígitos del 0 al 7 (ejemplo: 764)";
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

    ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
    if (!nav.exito) {
        fclose(archivo);
        return "Error: No se encontró la ruta '" + ruta + "'";
    }

    int uid_actual     = SessionManager::currentSession.uid;
    bool es_root       = SessionManager::currentSession.isRoot;
    int nuevos_permisos = ugoAPermisos(ugo);

    Inode inodo = leerInodo(archivo, super, nav.inodo_actual);

    // Sin -r: solo el propietario o root puede cambiar
    if (!recursivo) {
        if (!es_root && inodo.i_uid != uid_actual) {
            fclose(archivo);
            return "Error: No tiene permisos para cambiar los permisos de '" + ruta + "'";
        }
        inodo.i_perm  = nuevos_permisos;
        inodo.i_mtime = std::time(nullptr);
        escribirInodo(archivo, super, nav.inodo_actual, inodo);
    } else {
        chmodRecursivoDir(archivo, super, nav.inodo_actual,
                          nuevos_permisos, uid_actual, es_root);
    }

    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);

    std::string modo = recursivo ? " (recursivo)" : "";
    return "Permisos de '" + ruta + "' cambiados a " + ugo + modo + " exitosamente";
}

#endif