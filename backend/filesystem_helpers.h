#ifndef FILESYSTEM_HELPERS_H
#define FILESYSTEM_HELPERS_H

#include <fstream>
#include <string>
#include <cstring>
#include "structures.h"
#include "mount.h"

struct ResultadoNavegacion {
    bool exito;
    int inodo_actual;
    int inodo_padre;
    std::string mensaje_error;
    
    ResultadoNavegacion() : exito(false), inodo_actual(-1), inodo_padre(-1), mensaje_error("") {}
};

inline int encontrarInodoLibre(FILE* archivo, const Superblock& super) {
    fseek(archivo, super.s_bm_inode_start, SEEK_SET);
    
    for (int i = 0; i < super.s_inodes_count; i++) {
        char bit;
        fread(&bit, 1, 1, archivo);
        
        if (bit == '0') {
            return i;
        }
    }
    
    return -1;
}

inline int encontrarBloqueLibre(FILE* archivo, const Superblock& super) {
    fseek(archivo, super.s_bm_block_start, SEEK_SET);
    
    for (int i = 0; i < super.s_blocks_count; i++) {
        char bit;
        fread(&bit, 1, 1, archivo);
        
        if (bit == '0') {
            return i;
        }
    }
    
    return -1;
}

inline bool marcarInodoUsado(FILE* archivo, const Superblock& super, int indice_inodo) {
    if (indice_inodo < 0 || indice_inodo >= super.s_inodes_count) {
        return false;
    }
    
    fseek(archivo, super.s_bm_inode_start + indice_inodo, SEEK_SET);
    
    char usado = '1';
    fwrite(&usado, 1, 1, archivo);
    
    return true;
}

inline bool marcarBloqueUsado(FILE* archivo, const Superblock& super, int indice_bloque) {
    if (indice_bloque < 0 || indice_bloque >= super.s_blocks_count) {
        return false;
    }
    
    fseek(archivo, super.s_bm_block_start + indice_bloque, SEEK_SET);

    char usado = '1';
    fwrite(&usado, 1, 1, archivo);
    
    return true;
}

inline Inode leerInodo(FILE* archivo, const Superblock& super, int indice_inodo) {
    Inode inodo;
    
    int posicion = super.s_inode_start + (indice_inodo * sizeof(Inode));
    
    fseek(archivo, posicion, SEEK_SET);
    fread(&inodo, sizeof(Inode), 1, archivo);
    
    return inodo;
}

inline bool escribirInodo(FILE* archivo, const Superblock& super, int indice_inodo, const Inode& inodo) {
    if (indice_inodo < 0 || indice_inodo >= super.s_inodes_count) {
        return false;
    }
    
    int posicion = super.s_inode_start + (indice_inodo * sizeof(Inode));
    
    fseek(archivo, posicion, SEEK_SET);
    fwrite(&inodo, sizeof(Inode), 1, archivo);
    
    return true;
}

inline FolderBlock leerBloqueCarpeta(FILE* archivo, const Superblock& super, int indice_bloque) {
    FolderBlock bloque;
    
    int posicion = super.s_block_start + (indice_bloque * 64);
    
    fseek(archivo, posicion, SEEK_SET);
    fread(&bloque, sizeof(FolderBlock), 1, archivo);
    
    return bloque;
}

inline bool escribirBloqueCarpeta(FILE* archivo, const Superblock& super, int indice_bloque, const FolderBlock& bloque) {
    if (indice_bloque < 0 || indice_bloque >= super.s_blocks_count) {
        return false;
    }
    
    int posicion = super.s_block_start + (indice_bloque * 64);
    
    fseek(archivo, posicion, SEEK_SET);
    fwrite(&bloque, sizeof(FolderBlock), 1, archivo);
    
    return true;
}

inline FileBlock leerBloqueArchivo(FILE* archivo, const Superblock& super, int indice_bloque) {
    FileBlock bloque;
    
    int posicion = super.s_block_start + (indice_bloque * 64);

    fseek(archivo, posicion, SEEK_SET);
    fread(&bloque, sizeof(FileBlock), 1, archivo);
    
    return bloque;
}

inline bool escribirBloqueArchivo(FILE* archivo, const Superblock& super, int indice_bloque, const FileBlock& bloque) {
    if (indice_bloque < 0 || indice_bloque >= super.s_blocks_count) {
        return false;
    }
    
    int posicion = super.s_block_start + (indice_bloque * 64);
    
    fseek(archivo, posicion, SEEK_SET);
    fwrite(&bloque, sizeof(FileBlock), 1, archivo);
    
    return true;
}

inline bool actualizarSuperblock(FILE* archivo, int inicio_particion, const Superblock& super) {
    fseek(archivo, inicio_particion, SEEK_SET);
    fwrite(&super, sizeof(Superblock), 1, archivo);
    return true;
}

inline std::vector<std::string> dividirRuta(const std::string& ruta) {
    std::vector<std::string> componentes;
    std::string componente_actual = "";
    
    for (size_t i = 0; i < ruta.length(); i++) {
        if (ruta[i] == '/') {
            if (!componente_actual.empty()) {
                componentes.push_back(componente_actual);
                componente_actual = "";
            }
        } else {
            componente_actual += ruta[i];
        }
    }
    
    if (!componente_actual.empty()) {
        componentes.push_back(componente_actual);
    }
    
    return componentes;
}

inline int buscarEnBloqueCarpeta(FILE* archivo, const Superblock& super, 
                                  int indice_bloque, const std::string& nombre) {
    FolderBlock bloque = leerBloqueCarpeta(archivo, super, indice_bloque);
    
    for (int i = 0; i < 4; i++) {
        if (bloque.b_content[i].b_inodo != -1) {
            std::string nombre_entrada(bloque.b_content[i].b_name);
            
            if (nombre_entrada.substr(0, nombre.length()) == nombre) {
                return bloque.b_content[i].b_inodo;
            }
        }
    }
    
    return -1;
}

inline ResultadoNavegacion navegarRuta(FILE* archivo, const Superblock& super, 
                                        const std::string& ruta, bool crear_faltantes = false) {
    ResultadoNavegacion resultado;

    if (ruta.empty() || ruta[0] != '/') {
        resultado.mensaje_error = "Error: La ruta debe comenzar con /";
        return resultado;
    }
    
    if (ruta == "/") {
        resultado.exito = true;
        resultado.inodo_actual = 0;
        resultado.inodo_padre = 0;
        return resultado;
    }
    
    std::vector<std::string> componentes = dividirRuta(ruta);
    
    if (componentes.empty()) {
        resultado.exito = true;
        resultado.inodo_actual = 0;
        resultado.inodo_padre = 0;
        return resultado;
    }

    int inodo_actual = 0;
    int inodo_padre = 0;
    
    for (size_t i = 0; i < componentes.size(); i++) {
        const std::string& componente = componentes[i];
        Inode inodo = leerInodo(archivo, super, inodo_actual);
        
        if (inodo.i_type != '1') {
            resultado.mensaje_error = "Error: '" + componente + "' no es una carpeta";
            return resultado;
        }

        bool encontrado = false;
        int siguiente_inodo = -1;
        
        for (int j = 0; j < 12 && inodo.i_block[j] != -1; j++) {
            siguiente_inodo = buscarEnBloqueCarpeta(archivo, super, inodo.i_block[j], componente);
            
            if (siguiente_inodo != -1) {
                encontrado = true;
                break;
            }
        }
        
        if (!encontrado) {
            if (!crear_faltantes) {
                resultado.mensaje_error = "Error: No existe '" + componente + "' en la ruta";
                return resultado;
            }
            resultado.mensaje_error = "Error: No existe '" + componente + "' (creación no implementada aún)";
            return resultado;
        }
        inodo_padre = inodo_actual;
        inodo_actual = siguiente_inodo;
    }
    resultado.exito = true;
    resultado.inodo_actual = inodo_actual;
    resultado.inodo_padre = inodo_padre;
    
    return resultado;
}

inline std::string obtenerNombreArchivo(const std::string& ruta) {
    size_t ultima_barra = ruta.find_last_of('/');
    
    if (ultima_barra == std::string::npos) {
        return ruta;
    }
    
    return ruta.substr(ultima_barra + 1);
}

inline std::string obtenerRutaPadre(const std::string& ruta) {
    size_t ultima_barra = ruta.find_last_of('/');
    
    if (ultima_barra == std::string::npos || ultima_barra == 0) {
        return "/";
    }
    
    return ruta.substr(0, ultima_barra);
}
inline bool agregarEntradaEnBloque(FILE* archivo, const Superblock& super, 
                                    int indice_bloque, const std::string& nombre, 
                                    int inodo_destino) {
    FolderBlock bloque = leerBloqueCarpeta(archivo, super, indice_bloque);
    
    for (int i = 0; i < 4; i++) {
        if (bloque.b_content[i].b_inodo == -1) {
            std::strncpy(bloque.b_content[i].b_name, nombre.c_str(), 12);
            bloque.b_content[i].b_inodo = inodo_destino;
            escribirBloqueCarpeta(archivo, super, indice_bloque, bloque);
            return true;
        }
    }
    
    return false;  // Bloque lleno
}

inline bool crearCarpeta(FILE* archivo, Superblock& super, int inodo_padre, 
                         const std::string& nombre, int uid, int gid) {
    int nuevo_inodo = encontrarInodoLibre(archivo, super);
    if (nuevo_inodo == -1) {
        return false;
    }
    
    int nuevo_bloque = encontrarBloqueLibre(archivo, super);
    if (nuevo_bloque == -1) {
        return false;
    }
    
    Inode inodo_carpeta;
    inodo_carpeta.i_uid = uid;
    inodo_carpeta.i_gid = gid;
    inodo_carpeta.i_size = 0;
    inodo_carpeta.i_atime = std::time(nullptr);
    inodo_carpeta.i_ctime = std::time(nullptr);
    inodo_carpeta.i_mtime = std::time(nullptr);
    inodo_carpeta.i_type = '1';
    inodo_carpeta.i_perm = 664;
    
    for (int i = 0; i < 15; i++) {
        inodo_carpeta.i_block[i] = -1;
    }
    inodo_carpeta.i_block[0] = nuevo_bloque;
    
    FolderBlock bloque_carpeta;
    
    std::strncpy(bloque_carpeta.b_content[0].b_name, ".", 12);
    bloque_carpeta.b_content[0].b_inodo = nuevo_inodo;
    
    std::strncpy(bloque_carpeta.b_content[1].b_name, "..", 12);
    bloque_carpeta.b_content[1].b_inodo = inodo_padre;
    
    bloque_carpeta.b_content[2].b_inodo = -1;
    bloque_carpeta.b_content[3].b_inodo = -1;
    
    escribirInodo(archivo, super, nuevo_inodo, inodo_carpeta);
    escribirBloqueCarpeta(archivo, super, nuevo_bloque, bloque_carpeta);
    
    marcarInodoUsado(archivo, super, nuevo_inodo);
    marcarBloqueUsado(archivo, super, nuevo_bloque);
    
    Inode inodo_padre_obj = leerInodo(archivo, super, inodo_padre);
    bool entrada_agregada = false;
    
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        if (agregarEntradaEnBloque(archivo, super, inodo_padre_obj.i_block[i], nombre, nuevo_inodo)) {
            entrada_agregada = true;
            break;
        }
    }
    
    if (!entrada_agregada) {
        int slot_libre = -1;
        for (int i = 0; i < 12; i++) {
            if (inodo_padre_obj.i_block[i] == -1) {
                slot_libre = i;
                break;
            }
        }
        
        if (slot_libre == -1) {
            return false;
        }
        
        int bloque_nuevo_padre = encontrarBloqueLibre(archivo, super);
        if (bloque_nuevo_padre == -1) {
            return false;
        }
        
        FolderBlock bloque_vacio;
        for (int i = 0; i < 4; i++) {
            bloque_vacio.b_content[i].b_inodo = -1;
        }

        std::strncpy(bloque_vacio.b_content[0].b_name, nombre.c_str(), 12);
        bloque_vacio.b_content[0].b_inodo = nuevo_inodo;
        
        escribirBloqueCarpeta(archivo, super, bloque_nuevo_padre, bloque_vacio);
        marcarBloqueUsado(archivo, super, bloque_nuevo_padre);

        inodo_padre_obj.i_block[slot_libre] = bloque_nuevo_padre;
        escribirInodo(archivo, super, inodo_padre, inodo_padre_obj);
        super.s_free_blocks_count--;
    }
    
    super.s_free_inodes_count--;
    super.s_free_blocks_count--;
    
    return true;
}

inline bool escribirBloqueApuntadores(FILE* archivo, const Superblock& super, 
                                       int indice_bloque, const PointerBlock& bloque) {
    if (indice_bloque < 0 || indice_bloque >= super.s_blocks_count) {
        return false;
    }
    
    int posicion = super.s_block_start + (indice_bloque * 64);
    fseek(archivo, posicion, SEEK_SET);
    fwrite(&bloque, sizeof(PointerBlock), 1, archivo);
    
    return true;
}

inline PointerBlock leerBloqueApuntadores(FILE* archivo, const Superblock& super, int indice_bloque) {
    PointerBlock bloque;
    
    int posicion = super.s_block_start + (indice_bloque * 64);
    fseek(archivo, posicion, SEEK_SET);
    fread(&bloque, sizeof(PointerBlock), 1, archivo);
    
    return bloque;
}

inline int obtenerPermisosCategoria(int permisos_octales, int categoria) {
    int divisor = 1;
    for (int i = 0; i < (2 - categoria); i++) {
        divisor *= 10;
    }
    
    int digito = (permisos_octales / divisor) % 10;
    return digito;
}

inline bool verificarPermiso(const Inode& inodo, int uid_actual, int gid_actual, 
                              bool es_root, int permiso_tipo) {
    // Root tiene todos los permisos
    if (es_root) {
        return true;
    }
    
    int categoria_permisos = 0;
    
    // Determinar categoría del usuario
    if (inodo.i_uid == uid_actual) {
        // Es el propietario (User)
        categoria_permisos = obtenerPermisosCategoria(inodo.i_perm, 0);
    } else if (inodo.i_gid == gid_actual) {
        // Es del mismo grupo (Group)
        categoria_permisos = obtenerPermisosCategoria(inodo.i_perm, 1);
    } else {
        // Otros usuarios (Other)
        categoria_permisos = obtenerPermisosCategoria(inodo.i_perm, 2);
    }
    return (categoria_permisos & permiso_tipo) != 0;
}

// Constantes para tipos de permisos
#define PERMISO_LECTURA 4
#define PERMISO_ESCRITURA 2
#define PERMISO_EJECUCION 1

#endif