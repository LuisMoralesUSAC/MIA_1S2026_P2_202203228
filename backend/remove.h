#ifndef REMOVE_H
#define REMOVE_H

#include <string>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

// Libera todos los bloques directos e indirectos de un inodo
inline void liberarBloquesInodo(FILE* archivo, Superblock& super, const Inode& inodo) {
    // Bloques directos
    for (int i = 0; i < 12; i++) {
        if (inodo.i_block[i] == -1) continue;
        fseek(archivo, super.s_bm_block_start + inodo.i_block[i], SEEK_SET);
        char libre = '0';
        fwrite(&libre, 1, 1, archivo);
        super.s_free_blocks_count++;
    }

    // Indirecto simple
    if (inodo.i_block[12] != -1) {
        PointerBlock pb = leerBloqueApuntadores(archivo, super, inodo.i_block[12]);
        for (int i = 0; i < 16; i++) {
            if (pb.b_pointers[i] == -1) continue;
            fseek(archivo, super.s_bm_block_start + pb.b_pointers[i], SEEK_SET);
            char libre = '0';
            fwrite(&libre, 1, 1, archivo);
            super.s_free_blocks_count++;
        }
        fseek(archivo, super.s_bm_block_start + inodo.i_block[12], SEEK_SET);
        char libre = '0';
        fwrite(&libre, 1, 1, archivo);
        super.s_free_blocks_count++;
    }

    // Indirecto doble
    if (inodo.i_block[13] != -1) {
        PointerBlock pb1 = leerBloqueApuntadores(archivo, super, inodo.i_block[13]);
        for (int i = 0; i < 16; i++) {
            if (pb1.b_pointers[i] == -1) continue;
            PointerBlock pb2 = leerBloqueApuntadores(archivo, super, pb1.b_pointers[i]);
            for (int j = 0; j < 16; j++) {
                if (pb2.b_pointers[j] == -1) continue;
                fseek(archivo, super.s_bm_block_start + pb2.b_pointers[j], SEEK_SET);
                char libre = '0';
                fwrite(&libre, 1, 1, archivo);
                super.s_free_blocks_count++;
            }
            fseek(archivo, super.s_bm_block_start + pb1.b_pointers[i], SEEK_SET);
            char libre = '0';
            fwrite(&libre, 1, 1, archivo);
            super.s_free_blocks_count++;
        }
        fseek(archivo, super.s_bm_block_start + inodo.i_block[13], SEEK_SET);
        char libre = '0';
        fwrite(&libre, 1, 1, archivo);
        super.s_free_blocks_count++;
    }

    // Indirecto triple
    if (inodo.i_block[14] != -1) {
        PointerBlock pb1 = leerBloqueApuntadores(archivo, super, inodo.i_block[14]);
        for (int i = 0; i < 16; i++) {
            if (pb1.b_pointers[i] == -1) continue;
            PointerBlock pb2 = leerBloqueApuntadores(archivo, super, pb1.b_pointers[i]);
            for (int j = 0; j < 16; j++) {
                if (pb2.b_pointers[j] == -1) continue;
                PointerBlock pb3 = leerBloqueApuntadores(archivo, super, pb2.b_pointers[j]);
                for (int k = 0; k < 16; k++) {
                    if (pb3.b_pointers[k] == -1) continue;
                    fseek(archivo, super.s_bm_block_start + pb3.b_pointers[k], SEEK_SET);
                    char libre = '0';
                    fwrite(&libre, 1, 1, archivo);
                    super.s_free_blocks_count++;
                }
                fseek(archivo, super.s_bm_block_start + pb2.b_pointers[j], SEEK_SET);
                char libre = '0';
                fwrite(&libre, 1, 1, archivo);
                super.s_free_blocks_count++;
            }
            fseek(archivo, super.s_bm_block_start + pb1.b_pointers[i], SEEK_SET);
            char libre = '0';
            fwrite(&libre, 1, 1, archivo);
            super.s_free_blocks_count++;
        }
        fseek(archivo, super.s_bm_block_start + inodo.i_block[14], SEEK_SET);
        char libre = '0';
        fwrite(&libre, 1, 1, archivo);
        super.s_free_blocks_count++;
    }
}

// Verifica recursivamente si todos los elementos de una carpeta
inline bool verificarPermisosRecursivo(FILE* archivo, const Superblock& super,
                                        int inodo_num, int uid, int gid, bool es_root) {
    if (inodo_num < 0 || inodo_num >= super.s_inodes_count) {
        return false;
    }

    Inode inodo = leerInodo(archivo, super, inodo_num);

    if (!verificarPermiso(inodo, uid, gid, es_root, PERMISO_ESCRITURA)) {
        return false;
    }

    if (inodo.i_type == '1') {
        for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
            FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo.i_block[i]);

            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;

                char buf[13] = {0};
                strncpy(buf, fb.b_content[j].b_name, 12);
                std::string nombre(buf);

                if (nombre == "." || nombre == "..") continue;

                int hijo_inodo = fb.b_content[j].b_inodo;
                if (hijo_inodo < 0 || hijo_inodo >= super.s_inodes_count) {
                    continue;
                }

                if (!verificarPermisosRecursivo(archivo, super,
                        hijo_inodo, uid, gid, es_root)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// Elimina recursivamente el contenido de una carpeta y luego la carpeta misma
inline void eliminarRecursivo(FILE* archivo, Superblock& super, int inodo_num) {
    if (inodo_num < 0 || inodo_num >= super.s_inodes_count) {
        return;
    }

    Inode inodo = leerInodo(archivo, super, inodo_num);

    if (inodo.i_type == '1') {
        for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
            FolderBlock fb = leerBloqueCarpeta(archivo, super, inodo.i_block[i]);

            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;

                char buf[13] = {0};
                strncpy(buf, fb.b_content[j].b_name, 12);
                std::string nombre(buf);

                if (nombre == "." || nombre == "..") continue;

                int hijo_inodo = fb.b_content[j].b_inodo;
                if (hijo_inodo < 0 || hijo_inodo >= super.s_inodes_count) {
                    continue;
                }

                eliminarRecursivo(archivo, super, hijo_inodo);
            }
        }

        for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
            fseek(archivo, super.s_bm_block_start + inodo.i_block[i], SEEK_SET);
            char libre = '0';
            fwrite(&libre, 1, 1, archivo);
            super.s_free_blocks_count++;
        }
    } else {
        liberarBloquesInodo(archivo, super, inodo);
    }

    fseek(archivo, super.s_bm_inode_start + inodo_num, SEEK_SET);
    char libre = '0';
    fwrite(&libre, 1, 1, archivo);
    super.s_free_inodes_count++;
}

// Elimina la entrada de un inodo dentro del bloque de carpeta del padre
inline void eliminarEntradaPadre(FILE* archivo, const Superblock& super,
                                  int inodo_padre, int inodo_hijo) {
    Inode padre = leerInodo(archivo, super, inodo_padre);

    for (int i = 0; i < 12 && padre.i_block[i] != -1; i++) {
        FolderBlock fb = leerBloqueCarpeta(archivo, super, padre.i_block[i]);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == inodo_hijo) {
                fb.b_content[j].b_inodo = -1;
                memset(fb.b_content[j].b_name, 0, 12);
                escribirBloqueCarpeta(archivo, super, padre.i_block[i], fb);
                return;
            }
        }
    }
}

inline std::string comandoRemove(const std::string& ruta) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }

    if (ruta.empty() || ruta[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }

    if (ruta == "/") {
        return "Error: No se puede eliminar el directorio raíz";
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

    int uid = SessionManager::currentSession.uid;
    int gid = SessionManager::currentSession.gid;
    bool es_root = SessionManager::currentSession.isRoot;

    // Verificar permisos recursivamente antes de eliminar nada
    if (!verificarPermisosRecursivo(archivo, super, nav.inodo_actual, uid, gid, es_root)) {
        fclose(archivo);
        return "Error: No tiene permisos de escritura sobre todos los elementos en '" + ruta + "'";
    }

    // Eliminar recursivamente
    eliminarRecursivo(archivo, super, nav.inodo_actual);

    // Eliminar entrada en el padre
    eliminarEntradaPadre(archivo, super, nav.inodo_padre, nav.inodo_actual);

    // Actualizar superblock
    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);

    return "'" + ruta + "' eliminado exitosamente";
}

#endif