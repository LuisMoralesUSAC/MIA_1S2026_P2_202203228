#ifndef CAT_H
#define CAT_H

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

// Leer contenido de un archivo siguiendo todos los apuntadores
inline std::string leerContenidoArchivo(FILE* archivo, const Superblock& super, const Inode& inodo) {
    std::string contenido = "";
    int bytes_leidos = 0;
    int bytes_totales = inodo.i_size;
    
    for (int i = 0; i < 12 && inodo.i_block[i] != -1 && bytes_leidos < bytes_totales; i++) {
        FileBlock bloque = leerBloqueArchivo(archivo, super, inodo.i_block[i]);
        
        int bytes_a_leer = (bytes_totales - bytes_leidos < 64) ? (bytes_totales - bytes_leidos) : 64;
        contenido += std::string(bloque.b_content, bytes_a_leer);
        bytes_leidos += bytes_a_leer;
    }
    
    if (inodo.i_block[12] != -1 && bytes_leidos < bytes_totales) {
        PointerBlock punteros = leerBloqueApuntadores(archivo, super, inodo.i_block[12]);
        
        for (int i = 0; i < 16 && punteros.b_pointers[i] != -1 && bytes_leidos < bytes_totales; i++) {
            FileBlock bloque = leerBloqueArchivo(archivo, super, punteros.b_pointers[i]);
            
            int bytes_a_leer = (bytes_totales - bytes_leidos < 64) ? (bytes_totales - bytes_leidos) : 64;
            contenido += std::string(bloque.b_content, bytes_a_leer);
            bytes_leidos += bytes_a_leer;
        }
    }
    
    if (inodo.i_block[13] != -1 && bytes_leidos < bytes_totales) {
        PointerBlock punteros_nivel1 = leerBloqueApuntadores(archivo, super, inodo.i_block[13]);
        
        for (int i = 0; i < 16 && punteros_nivel1.b_pointers[i] != -1 && bytes_leidos < bytes_totales; i++) {
            PointerBlock punteros_nivel2 = leerBloqueApuntadores(archivo, super, punteros_nivel1.b_pointers[i]);
            
            for (int j = 0; j < 16 && punteros_nivel2.b_pointers[j] != -1 && bytes_leidos < bytes_totales; j++) {
                FileBlock bloque = leerBloqueArchivo(archivo, super, punteros_nivel2.b_pointers[j]);
                
                int bytes_a_leer = (bytes_totales - bytes_leidos < 64) ? (bytes_totales - bytes_leidos) : 64;
                contenido += std::string(bloque.b_content, bytes_a_leer);
                bytes_leidos += bytes_a_leer;
            }
        }
    }
    
    if (inodo.i_block[14] != -1 && bytes_leidos < bytes_totales) {
        PointerBlock punteros_nivel1 = leerBloqueApuntadores(archivo, super, inodo.i_block[14]);
        
        for (int i = 0; i < 16 && punteros_nivel1.b_pointers[i] != -1 && bytes_leidos < bytes_totales; i++) {
            PointerBlock punteros_nivel2 = leerBloqueApuntadores(archivo, super, punteros_nivel1.b_pointers[i]);
            
            for (int j = 0; j < 16 && punteros_nivel2.b_pointers[j] != -1 && bytes_leidos < bytes_totales; j++) {
                PointerBlock punteros_nivel3 = leerBloqueApuntadores(archivo, super, punteros_nivel2.b_pointers[j]);
                
                for (int k = 0; k < 16 && punteros_nivel3.b_pointers[k] != -1 && bytes_leidos < bytes_totales; k++) {
                    FileBlock bloque = leerBloqueArchivo(archivo, super, punteros_nivel3.b_pointers[k]);
                    
                    int bytes_a_leer = (bytes_totales - bytes_leidos < 64) ? (bytes_totales - bytes_leidos) : 64;
                    contenido += std::string(bloque.b_content, bytes_a_leer);
                    bytes_leidos += bytes_a_leer;
                }
            }
        }
    }
    
    return contenido;
}

inline std::string comandoCat(const std::vector<std::string>& rutas_archivos) {
    // Validar sesión activa
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }
    
    if (rutas_archivos.empty()) {
        return "Error: Debe especificar al menos un archivo";
    }
    
    // Obtener partición montada
    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;
    
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }
    
    // Abrir archivo del disco
    FILE* archivo = fopen(particion.path.c_str(), "rb");
    if (!archivo) {
        return "Error: No se pudo abrir el disco";
    }
    
    // Leer superblock
    Superblock super;
    fseek(archivo, particion.start, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);
    
    std::string resultado = "";
    
    // Procesar cada archivo
    for (size_t i = 0; i < rutas_archivos.size(); i++) {
        const std::string& ruta = rutas_archivos[i];
        
        // Validar ruta
        if (ruta.empty() || ruta[0] != '/') {
            resultado += "Error: La ruta '" + ruta + "' debe comenzar con /\n";
            continue;
        }
        
        // Navegar a la ruta del archivo
        ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
        
        if (!nav.exito) {
            resultado += "Error: No se encontró el archivo '" + ruta + "'\n";
            continue;
        }
        
        // Leer el inodo
        Inode inodo = leerInodo(archivo, super, nav.inodo_actual);
        
        // Verificar que sea un archivo (no carpeta)
        if (inodo.i_type != '0') {
            resultado += "Error: '" + ruta + "' no es un archivo\n";
            continue;
        }
        
        // Validar permisos de lectura
        if (!verificarPermiso(inodo, SessionManager::currentSession.uid, 
                              SessionManager::currentSession.gid,
                              SessionManager::currentSession.isRoot,
                              PERMISO_LECTURA)) {
            resultado += "Error: No tiene permisos de lectura sobre '" + ruta + "'\n";
            continue;
        }
        
        // Leer contenido
        std::string contenido = leerContenidoArchivo(archivo, super, inodo);
        
        if (rutas_archivos.size() > 1) {
            resultado += "=== " + ruta + " ===\n";
        }
        
        resultado += contenido;
        
        if (rutas_archivos.size() > 1 && i < rutas_archivos.size() - 1) {
            resultado += "\n\n";
        }
    }
    
    fclose(archivo);
    return resultado;
}

#endif