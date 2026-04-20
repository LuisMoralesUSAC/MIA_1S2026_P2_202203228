#ifndef MOVE_H
#define MOVE_H

#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

inline std::string comandoMove(const std::string& ruta_origen,
                                const std::string& ruta_destino) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }

    if (ruta_origen.empty() || ruta_origen[0] != '/') {
        return "Error: La ruta origen debe comenzar con /";
    }

    if (ruta_destino.empty() || ruta_destino[0] != '/') {
        return "Error: La ruta destino debe comenzar con /";
    }

    if (ruta_origen == "/") {
        return "Error: No se puede mover el directorio raíz";
    }

    // Verificar que el destino no sea subdirectorio del origen
    if (ruta_destino.find(ruta_origen) == 0) {
        return "Error: El destino no puede ser subdirectorio del origen";
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

    // Verificar que el origen existe
    ResultadoNavegacion nav_origen = navegarRuta(archivo, super, ruta_origen, false);
    if (!nav_origen.exito) {
        fclose(archivo);
        return "Error: No se encontró el origen '" + ruta_origen + "'";
    }

    // Verificar permiso de escritura sobre el origen
    Inode inodo_origen = leerInodo(archivo, super, nav_origen.inodo_actual);
    if (!verificarPermiso(inodo_origen,
                          SessionManager::currentSession.uid,
                          SessionManager::currentSession.gid,
                          SessionManager::currentSession.isRoot,
                          PERMISO_ESCRITURA)) {
        fclose(archivo);
        return "Error: No tiene permisos de escritura sobre '" + ruta_origen + "'";
    }

    // Verificar que el destino existe y es carpeta
    ResultadoNavegacion nav_destino = navegarRuta(archivo, super, ruta_destino, false);
    if (!nav_destino.exito) {
        fclose(archivo);
        return "Error: No se encontró el destino '" + ruta_destino + "'";
    }

    Inode inodo_destino = leerInodo(archivo, super, nav_destino.inodo_actual);
    if (inodo_destino.i_type != '1') {
        fclose(archivo);
        return "Error: El destino '" + ruta_destino + "' no es una carpeta";
    }

    // Verificar permiso de escritura en destino
    if (!verificarPermiso(inodo_destino,
                          SessionManager::currentSession.uid,
                          SessionManager::currentSession.gid,
                          SessionManager::currentSession.isRoot,
                          PERMISO_ESCRITURA)) {
        fclose(archivo);
        return "Error: No tiene permisos de escritura en el destino";
    }

    std::string nombre = obtenerNombreArchivo(ruta_origen);

    // Verificar que no exista ya un elemento con ese nombre en el destino
    inodo_destino = leerInodo(archivo, super, nav_destino.inodo_actual);
    for (int i = 0; i < 12 && inodo_destino.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_destino.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            char buf[13] = {0};
            strncpy(buf, fb.b_content[j].b_name, 12);
            if (std::string(buf) == nombre) {
                fclose(archivo);
                return "Error: Ya existe '" + nombre + "' en el destino";
            }
        }
    }

    // Misma partición: solo cambiar referencias
    // 1. Agregar entrada en el destino
    inodo_destino = leerInodo(archivo, super, nav_destino.inodo_actual);
    bool agregado = false;

    for (int i = 0; i < 12 && inodo_destino.i_block[i] != -1 && !agregado; i++) {
        if (agregarEntradaEnBloque(archivo, super, inodo_destino.i_block[i],
                                   nombre, nav_origen.inodo_actual)) {
            agregado = true;
        }
    }

    if (!agregado) {
        // Necesita nuevo bloque en el destino
        int slot_libre = -1;
        for (int i = 0; i < 12; i++) {
            if (inodo_destino.i_block[i] == -1) { slot_libre = i; break; }
        }

        if (slot_libre == -1) {
            fclose(archivo);
            return "Error: No hay espacio en el directorio destino";
        }

        int nuevo_bloque = encontrarBloqueLibre(archivo, super);
        if (nuevo_bloque == -1) {
            fclose(archivo);
            return "Error: No hay bloques libres";
        }

        FolderBlock fb_vacio;
        for (int i = 0; i < 4; i++) fb_vacio.b_content[i].b_inodo = -1;
        strncpy(fb_vacio.b_content[0].b_name, nombre.c_str(), 12);
        fb_vacio.b_content[0].b_inodo = nav_origen.inodo_actual;

        escribirBloqueCarpeta(archivo, super, nuevo_bloque, fb_vacio);
        marcarBloqueUsado(archivo, super, nuevo_bloque);
        inodo_destino.i_block[slot_libre] = nuevo_bloque;
        escribirInodo(archivo, super, nav_destino.inodo_actual, inodo_destino);
        super.s_free_blocks_count--;
    }

    // 2. Eliminar entrada en el padre origen
    Inode inodo_padre_origen = leerInodo(archivo, super, nav_origen.inodo_padre);
    for (int i = 0; i < 12 && inodo_padre_origen.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_padre_origen.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo != nav_origen.inodo_actual) continue;

            fb.b_content[j].b_inodo = -1;
            memset(fb.b_content[j].b_name, 0, 12);
            escribirBloqueCarpeta(archivo, super, inodo_padre_origen.i_block[i], fb);
            goto entrada_eliminada;
        }
    }
    entrada_eliminada:

    // 3. Actualizar entrada '..' dentro del inodo movido si es carpeta
    if (inodo_origen.i_type == '1') {
        for (int i = 0; i < 12 && inodo_origen.i_block[i] != -1; i++) {
            FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_origen.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                char buf[13] = {0};
                strncpy(buf, fb.b_content[j].b_name, 12);
                if (std::string(buf) == "..") {
                    fb.b_content[j].b_inodo = nav_destino.inodo_actual;
                    escribirBloqueCarpeta(archivo, super, inodo_origen.i_block[i], fb);
                    goto punto_punto_actualizado;
                }
            }
        }
        punto_punto_actualizado:;
    }

    // 4. Actualizar tiempo de modificación
    inodo_origen.i_mtime = std::time(nullptr);
    escribirInodo(archivo, super, nav_origen.inodo_actual, inodo_origen);

    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);

    return "'" + ruta_origen + "' movido a '" + ruta_destino + "' exitosamente";
}

#endif