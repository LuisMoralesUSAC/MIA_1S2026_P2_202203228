#ifndef CHOWN_H
#define CHOWN_H

#include <string>
#include <sstream>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

// Obtiene el UID de un usuario leyendo users.txt (inodo 1)
inline int obtenerUidUsuario(FILE* archivo, const Superblock& super,
                              const std::string& nombre_usuario) {
    std::string contenido = "";

    Inode inodo_users = leerInodo(archivo, super, 1);
    for (int i = 0; i < 12 && inodo_users.i_block[i] != -1; i++) {
        FileBlock bloque = leerBloqueArchivo(archivo, super, inodo_users.i_block[i]);
        contenido += std::string(bloque.b_content, 64);
    }

    size_t pos_nulo = contenido.find('\0');
    if (pos_nulo != std::string::npos) contenido = contenido.substr(0, pos_nulo);

    std::istringstream ss(contenido);
    std::string linea;
    while (std::getline(ss, linea)) {
        if (linea.empty()) continue;
        std::istringstream ls(linea);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ls, token, ',')) tokens.push_back(token);
        if (tokens.size() >= 5 && tokens[1] == "U" &&
            tokens[3] == nombre_usuario && tokens[0] != "0") {
            return std::stoi(tokens[0]);
        }
    }
    return -1;
}

// Aplica chown a un inodo, opcionalmente recursivo
inline bool chownRecursivo(FILE* archivo, Superblock& super,
                            int inodo_num, int nuevo_uid,
                            int uid_actual, bool es_root) {
    Inode inodo = leerInodo(archivo, super, inodo_num);

    // Solo root o el propietario actual pueden cambiar
    if (!es_root && inodo.i_uid != uid_actual) {
        return false;
    }

    inodo.i_uid = nuevo_uid;
    inodo.i_mtime = std::time(nullptr);
    escribirInodo(archivo, super, inodo_num, inodo);
    return true;
}

inline void chownRecursivoDir(FILE* archivo, Superblock& super,
                               int inodo_num, int nuevo_uid,
                               int uid_actual, bool es_root,
                               std::vector<std::string>& errores,
                               const std::string& ruta) {
    Inode inodo = leerInodo(archivo, super, inodo_num);

    if (!es_root && inodo.i_uid != uid_actual) {
        errores.push_back("Sin permiso sobre: " + ruta);
        return;
    }

    inodo.i_uid = nuevo_uid;
    inodo.i_mtime = std::time(nullptr);
    escribirInodo(archivo, super, inodo_num, inodo);

    if (inodo.i_type != '1') return;

    for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            char buf[13] = {0};
            strncpy(buf, fb.b_content[j].b_name, 12);
            std::string nombre(buf);
            if (nombre == "." || nombre == "..") continue;

            std::string ruta_hijo = (ruta == "/") ? "/" + nombre : ruta + "/" + nombre;
            chownRecursivoDir(archivo, super, fb.b_content[j].b_inodo,
                              nuevo_uid, uid_actual, es_root, errores, ruta_hijo);
        }
    }
}

inline std::string comandoChown(const std::string& ruta,
                                 const std::string& nombre_usuario,
                                 bool recursivo) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }

    if (ruta.empty() || ruta[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }

    if (nombre_usuario.empty()) {
        return "Error: Debe especificar el parámetro -usuario";
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

    // Verificar que el usuario destino exista
    int nuevo_uid = obtenerUidUsuario(archivo, super, nombre_usuario);
    if (nuevo_uid == -1) {
        fclose(archivo);
        return "Error: El usuario '" + nombre_usuario + "' no existe";
    }

    // Navegar a la ruta
    ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
    if (!nav.exito) {
        fclose(archivo);
        return "Error: No se encontró la ruta '" + ruta + "'";
    }

    int uid_actual = SessionManager::currentSession.uid;
    bool es_root   = SessionManager::currentSession.isRoot;

    std::vector<std::string> errores;

    if (recursivo) {
        chownRecursivoDir(archivo, super, nav.inodo_actual,
                          nuevo_uid, uid_actual, es_root, errores, ruta);
    } else {
        if (!chownRecursivo(archivo, super, nav.inodo_actual,
                            nuevo_uid, uid_actual, es_root)) {
            fclose(archivo);
            return "Error: No tiene permisos para cambiar el propietario de '" + ruta + "'";
        }
    }

    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);

    if (!errores.empty()) {
        std::string msg = "Chown completado con advertencias:\n";
        for (const std::string& e : errores) msg += "  " + e + "\n";
        return msg;
    }

    std::string modo = recursivo ? " (recursivo)" : "";
    return "Propietario de '" + ruta + "' cambiado a '" + nombre_usuario + "'" + modo + " exitosamente";
}

#endif