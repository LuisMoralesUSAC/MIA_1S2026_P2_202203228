#ifndef COPY_H
#define COPY_H

#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

// Copia el contenido de bloques de un archivo a un nuevo inodo
inline bool copiarBloqueArchivo(FILE* archivo, Superblock& super,
                                 const Inode& inodo_origen, Inode& inodo_dest) {
    // Leer contenido completo del origen
    std::string contenido = "";
    int bytes_leidos = 0;
    int bytes_totales = inodo_origen.i_size;

    for (int i = 0; i < 12 && inodo_origen.i_block[i] != -1 && bytes_leidos < bytes_totales; i++) {
        FileBlock bloque = leerBloqueArchivo(archivo, super, inodo_origen.i_block[i]);
        int bytes_a_leer = std::min(64, bytes_totales - bytes_leidos);
        contenido += std::string(bloque.b_content, bytes_a_leer);
        bytes_leidos += bytes_a_leer;
    }

    if (inodo_origen.i_block[12] != -1 && bytes_leidos < bytes_totales) {
        PointerBlock pb = leerBloqueApuntadores(archivo, super, inodo_origen.i_block[12]);
        for (int i = 0; i < 16 && pb.b_pointers[i] != -1 && bytes_leidos < bytes_totales; i++) {
            FileBlock bloque = leerBloqueArchivo(archivo, super, pb.b_pointers[i]);
            int bytes_a_leer = std::min(64, bytes_totales - bytes_leidos);
            contenido += std::string(bloque.b_content, bytes_a_leer);
            bytes_leidos += bytes_a_leer;
        }
    }

    // Escribir contenido en bloques del destino
    int bytes_restantes = contenido.length();
    int offset = 0;

    for (int i = 0; i < 12 && bytes_restantes > 0; i++) {
        int bloque_libre = encontrarBloqueLibre(archivo, super);
        if (bloque_libre == -1) return false;

        FileBlock bloque;
        memset(bloque.b_content, 0, 64);
        int bytes_a_escribir = std::min(64, bytes_restantes);
        memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);

        escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
        marcarBloqueUsado(archivo, super, bloque_libre);
        inodo_dest.i_block[i] = bloque_libre;

        offset += bytes_a_escribir;
        bytes_restantes -= bytes_a_escribir;
        super.s_free_blocks_count--;
    }

    if (bytes_restantes > 0) {
        int bloque_ptr = encontrarBloqueLibre(archivo, super);
        if (bloque_ptr == -1) return false;

        PointerBlock pb;
        for (int i = 0; i < 16 && bytes_restantes > 0; i++) {
            int bloque_libre = encontrarBloqueLibre(archivo, super);
            if (bloque_libre == -1) return false;

            FileBlock bloque;
            memset(bloque.b_content, 0, 64);
            int bytes_a_escribir = std::min(64, bytes_restantes);
            memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);

            escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
            marcarBloqueUsado(archivo, super, bloque_libre);
            pb.b_pointers[i] = bloque_libre;

            offset += bytes_a_escribir;
            bytes_restantes -= bytes_a_escribir;
            super.s_free_blocks_count--;
        }

        escribirBloqueApuntadores(archivo, super, bloque_ptr, pb);
        marcarBloqueUsado(archivo, super, bloque_ptr);
        inodo_dest.i_block[12] = bloque_ptr;
        super.s_free_blocks_count--;
    }

    return true;
}

// Copia recursivamente un archivo o carpeta al destino
// Retorna el inodo creado o -1 si fallo
inline int copiarRecursivo(FILE* archivo, Superblock& super,
                            int inodo_origen_num, int inodo_padre_dest,
                            const std::string& nombre,
                            int uid, int gid, bool es_root) {
    Inode inodo_origen = leerInodo(archivo, super, inodo_origen_num);

    // Verificar permiso de lectura sobre el origen
    if (!verificarPermiso(inodo_origen, uid, gid, es_root, PERMISO_LECTURA)) {
        return -1; // Sin permiso, se omite silenciosamente
    }

    if (inodo_origen.i_type == '0') {
        // Es archivo: crear nuevo inodo y copiar bloques
        int nuevo_inodo_num = encontrarInodoLibre(archivo, super);
        if (nuevo_inodo_num == -1) return -1;

        Inode nuevo_inodo;
        nuevo_inodo.i_uid  = uid;
        nuevo_inodo.i_gid  = gid;
        nuevo_inodo.i_size = inodo_origen.i_size;
        nuevo_inodo.i_atime = std::time(nullptr);
        nuevo_inodo.i_ctime = std::time(nullptr);
        nuevo_inodo.i_mtime = std::time(nullptr);
        nuevo_inodo.i_type = '0';
        nuevo_inodo.i_perm = inodo_origen.i_perm;
        for (int i = 0; i < 15; i++) nuevo_inodo.i_block[i] = -1;

        if (!copiarBloqueArchivo(archivo, super, inodo_origen, nuevo_inodo)) {
            return -1;
        }

        escribirInodo(archivo, super, nuevo_inodo_num, nuevo_inodo);
        marcarInodoUsado(archivo, super, nuevo_inodo_num);
        super.s_free_inodes_count--;

        // Agregar entrada en el padre destino
        Inode padre_dest = leerInodo(archivo, super, inodo_padre_dest);
        bool agregado = false;

        for (int i = 0; i < 12 && padre_dest.i_block[i] != -1 && !agregado; i++) {
            if (agregarEntradaEnBloque(archivo, super, padre_dest.i_block[i],
                                       nombre, nuevo_inodo_num)) {
                agregado = true;
            }
        }

        if (!agregado) {
            // Necesita nuevo bloque en el padre
            int slot_libre = -1;
            for (int i = 0; i < 12; i++) {
                if (padre_dest.i_block[i] == -1) { slot_libre = i; break; }
            }
            if (slot_libre == -1) return -1;

            int nuevo_bloque = encontrarBloqueLibre(archivo, super);
            if (nuevo_bloque == -1) return -1;

            FolderBlock fb_vacio;
            for (int i = 0; i < 4; i++) fb_vacio.b_content[i].b_inodo = -1;
            strncpy(fb_vacio.b_content[0].b_name, nombre.c_str(), 12);
            fb_vacio.b_content[0].b_inodo = nuevo_inodo_num;

            escribirBloqueCarpeta(archivo, super, nuevo_bloque, fb_vacio);
            marcarBloqueUsado(archivo, super, nuevo_bloque);
            padre_dest.i_block[slot_libre] = nuevo_bloque;
            escribirInodo(archivo, super, inodo_padre_dest, padre_dest);
            super.s_free_blocks_count--;
        }

        return nuevo_inodo_num;

    } else {
        // Es carpeta: crear nueva carpeta y copiar contenido
        int nuevo_inodo_num = encontrarInodoLibre(archivo, super);
        if (nuevo_inodo_num == -1) return -1;

        int nuevo_bloque_num = encontrarBloqueLibre(archivo, super);
        if (nuevo_bloque_num == -1) return -1;

        Inode nuevo_inodo;
        nuevo_inodo.i_uid  = uid;
        nuevo_inodo.i_gid  = gid;
        nuevo_inodo.i_size = 0;
        nuevo_inodo.i_atime = std::time(nullptr);
        nuevo_inodo.i_ctime = std::time(nullptr);
        nuevo_inodo.i_mtime = std::time(nullptr);
        nuevo_inodo.i_type = '1';
        nuevo_inodo.i_perm = inodo_origen.i_perm;
        for (int i = 0; i < 15; i++) nuevo_inodo.i_block[i] = -1;
        nuevo_inodo.i_block[0] = nuevo_bloque_num;

        // Inicializar bloque de carpeta con . y ..
        FolderBlock fb_nueva;
        for (int i = 0; i < 4; i++) fb_nueva.b_content[i].b_inodo = -1;
        strncpy(fb_nueva.b_content[0].b_name, ".", 12);
        fb_nueva.b_content[0].b_inodo = nuevo_inodo_num;
        strncpy(fb_nueva.b_content[1].b_name, "..", 12);
        fb_nueva.b_content[1].b_inodo = inodo_padre_dest;

        escribirInodo(archivo, super, nuevo_inodo_num, nuevo_inodo);
        escribirBloqueCarpeta(archivo, super, nuevo_bloque_num, fb_nueva);
        marcarInodoUsado(archivo, super, nuevo_inodo_num);
        marcarBloqueUsado(archivo, super, nuevo_bloque_num);
        super.s_free_inodes_count--;
        super.s_free_blocks_count--;

        // Agregar entrada en el padre destino
        Inode padre_dest = leerInodo(archivo, super, inodo_padre_dest);
        bool agregado = false;

        for (int i = 0; i < 12 && padre_dest.i_block[i] != -1 && !agregado; i++) {
            if (agregarEntradaEnBloque(archivo, super, padre_dest.i_block[i],
                                       nombre, nuevo_inodo_num)) {
                agregado = true;
            }
        }

        if (!agregado) {
            int slot_libre = -1;
            for (int i = 0; i < 12; i++) {
                if (padre_dest.i_block[i] == -1) { slot_libre = i; break; }
            }
            if (slot_libre == -1) return -1;

            int nuevo_bloque_padre = encontrarBloqueLibre(archivo, super);
            if (nuevo_bloque_padre == -1) return -1;

            FolderBlock fb_vacio;
            for (int i = 0; i < 4; i++) fb_vacio.b_content[i].b_inodo = -1;
            strncpy(fb_vacio.b_content[0].b_name, nombre.c_str(), 12);
            fb_vacio.b_content[0].b_inodo = nuevo_inodo_num;

            escribirBloqueCarpeta(archivo, super, nuevo_bloque_padre, fb_vacio);
            marcarBloqueUsado(archivo, super, nuevo_bloque_padre);
            padre_dest.i_block[slot_libre] = nuevo_bloque_padre;
            escribirInodo(archivo, super, inodo_padre_dest, padre_dest);
            super.s_free_blocks_count--;
        }

        // Copiar contenido recursivamente
        for (int i = 0; i < 12 && inodo_origen.i_block[i] != -1; i++) {
            FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo_origen.i_block[i]);
            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;

                char buf[13] = {0};
                strncpy(buf, fb.b_content[j].b_name, 12);
                std::string nombre_hijo(buf);
                if (nombre_hijo == "." || nombre_hijo == "..") continue;

                copiarRecursivo(archivo, super, fb.b_content[j].b_inodo,
                                nuevo_inodo_num, nombre_hijo, uid, gid, es_root);
            }
        }

        return nuevo_inodo_num;
    }
}

inline std::string comandoCopy(const std::string& ruta_origen,
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

    int resultado = copiarRecursivo(archivo, super,
                                    nav_origen.inodo_actual,
                                    nav_destino.inodo_actual,
                                    nombre,
                                    SessionManager::currentSession.uid,
                                    SessionManager::currentSession.gid,
                                    SessionManager::currentSession.isRoot);

    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);

    if (resultado == -1) {
        return "Error: No se pudo copiar '" + ruta_origen +
               "' (sin permisos de lectura o espacio insuficiente)";
    }

    return "'" + ruta_origen + "' copiado a '" + ruta_destino + "' exitosamente";
}

#endif