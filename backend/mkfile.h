#ifndef MKFILE_H
#define MKFILE_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

inline std::string generarContenido(int tamanio) {
    std::string patron = "0123456789";
    std::string contenido = "";
    
    for (int i = 0; i < tamanio; i++) {
        contenido += patron[i % 10];
    }
    
    return contenido;
}

// Función para leer archivo desde el sistema real
inline std::string leerArchivoReal(const std::string& ruta) {
    std::ifstream archivo(ruta, std::ios::binary);
    if (!archivo.is_open()) {
        return "";
    }
    
    std::string contenido((std::istreambuf_iterator<char>(archivo)),
                          std::istreambuf_iterator<char>());
    archivo.close();
    return contenido;
}

// Escribir contenido en bloques de archivo usando apuntadores directos e indirectos
inline bool escribirContenidoArchivo(FILE* archivo, Superblock& super, Inode& inodo, 
                                      const std::string& contenido) {
    int bytes_restantes = contenido.length();
    int offset = 0;

    for (int i = 0; i < 12 && bytes_restantes > 0; i++) {
        // Buscar bloque libre
        int bloque_libre = encontrarBloqueLibre(archivo, super);
        if (bloque_libre == -1) {
            return false;
        }
        
        // Preparar bloque de archivo
        FileBlock bloque;
        std::memset(bloque.b_content, 0, 64);
        
        int bytes_a_escribir = (bytes_restantes < 64) ? bytes_restantes : 64;
        std::memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);
        
        // Escribir bloque
        escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
        marcarBloqueUsado(archivo, super, bloque_libre);
        
        // Asignar al inodo
        inodo.i_block[i] = bloque_libre;
        
        offset += bytes_a_escribir;
        bytes_restantes -= bytes_a_escribir;
        super.s_free_blocks_count--;
    }
    
    if (bytes_restantes > 0) {
        // Buscar bloque para el apuntador
        int bloque_apuntador = encontrarBloqueLibre(archivo, super);
        if (bloque_apuntador == -1) {
            return false;
        }
        
        PointerBlock punteros;
        int punteros_usados = 0;
        
        // Llenar hasta 16 bloques de datos
        for (int i = 0; i < 16 && bytes_restantes > 0; i++) {
            int bloque_libre = encontrarBloqueLibre(archivo, super);
            if (bloque_libre == -1) {
                return false;
            }
            
            FileBlock bloque;
            std::memset(bloque.b_content, 0, 64);
            
            int bytes_a_escribir = (bytes_restantes < 64) ? bytes_restantes : 64;
            std::memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);
            
            escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
            marcarBloqueUsado(archivo, super, bloque_libre);
            
            punteros.b_pointers[i] = bloque_libre;
            punteros_usados++;
            
            offset += bytes_a_escribir;
            bytes_restantes -= bytes_a_escribir;
            super.s_free_blocks_count--;
        }
        
        // Escribir bloque de apuntadores
        escribirBloqueApuntadores(archivo, super, bloque_apuntador, punteros);
        marcarBloqueUsado(archivo, super, bloque_apuntador);
        inodo.i_block[12] = bloque_apuntador;
        super.s_free_blocks_count--;
    }
    
    if (bytes_restantes > 0) {
        // Buscar bloque para el apuntador principal
        int bloque_apuntador_principal = encontrarBloqueLibre(archivo, super);
        if (bloque_apuntador_principal == -1) {
            return false;
        }
        
        PointerBlock punteros_principales;
        
        // Hasta 16 bloques de apuntadores secundarios
        for (int i = 0; i < 16 && bytes_restantes > 0; i++) {
            // Buscar bloque para apuntador secundario
            int bloque_apuntador_secundario = encontrarBloqueLibre(archivo, super);
            if (bloque_apuntador_secundario == -1) {
                return false;
            }
            
            PointerBlock punteros_secundarios;
            
            // Hasta 16 bloques de datos por apuntador secundario
            for (int j = 0; j < 16 && bytes_restantes > 0; j++) {
                int bloque_libre = encontrarBloqueLibre(archivo, super);
                if (bloque_libre == -1) {
                    return false;
                }
                
                FileBlock bloque;
                std::memset(bloque.b_content, 0, 64);
                
                int bytes_a_escribir = (bytes_restantes < 64) ? bytes_restantes : 64;
                std::memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);
                
                escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
                marcarBloqueUsado(archivo, super, bloque_libre);
                
                punteros_secundarios.b_pointers[j] = bloque_libre;
                
                offset += bytes_a_escribir;
                bytes_restantes -= bytes_a_escribir;
                super.s_free_blocks_count--;
            }
            
            // Escribir apuntador secundario
            escribirBloqueApuntadores(archivo, super, bloque_apuntador_secundario, punteros_secundarios);
            marcarBloqueUsado(archivo, super, bloque_apuntador_secundario);
            punteros_principales.b_pointers[i] = bloque_apuntador_secundario;
            super.s_free_blocks_count--;
        }
        
        // Escribir apuntador principal
        escribirBloqueApuntadores(archivo, super, bloque_apuntador_principal, punteros_principales);
        marcarBloqueUsado(archivo, super, bloque_apuntador_principal);
        inodo.i_block[13] = bloque_apuntador_principal;
        super.s_free_blocks_count--;
    }
    
    if (bytes_restantes > 0) {
        // Similar a doble pero con un nivel más de indirección
        int bloque_apuntador_nivel1 = encontrarBloqueLibre(archivo, super);
        if (bloque_apuntador_nivel1 == -1) {
            return false;
        }
        
        PointerBlock punteros_nivel1;
        
        for (int i = 0; i < 16 && bytes_restantes > 0; i++) {
            int bloque_apuntador_nivel2 = encontrarBloqueLibre(archivo, super);
            if (bloque_apuntador_nivel2 == -1) {
                return false;
            }
            
            PointerBlock punteros_nivel2;
            
            for (int j = 0; j < 16 && bytes_restantes > 0; j++) {
                int bloque_apuntador_nivel3 = encontrarBloqueLibre(archivo, super);
                if (bloque_apuntador_nivel3 == -1) {
                    return false;
                }
                
                PointerBlock punteros_nivel3;
                
                for (int k = 0; k < 16 && bytes_restantes > 0; k++) {
                    int bloque_libre = encontrarBloqueLibre(archivo, super);
                    if (bloque_libre == -1) {
                        return false;
                    }
                    
                    FileBlock bloque;
                    std::memset(bloque.b_content, 0, 64);
                    
                    int bytes_a_escribir = (bytes_restantes < 64) ? bytes_restantes : 64;
                    std::memcpy(bloque.b_content, contenido.c_str() + offset, bytes_a_escribir);
                    
                    escribirBloqueArchivo(archivo, super, bloque_libre, bloque);
                    marcarBloqueUsado(archivo, super, bloque_libre);
                    
                    punteros_nivel3.b_pointers[k] = bloque_libre;
                    
                    offset += bytes_a_escribir;
                    bytes_restantes -= bytes_a_escribir;
                    super.s_free_blocks_count--;
                }
                
                escribirBloqueApuntadores(archivo, super, bloque_apuntador_nivel3, punteros_nivel3);
                marcarBloqueUsado(archivo, super, bloque_apuntador_nivel3);
                punteros_nivel2.b_pointers[j] = bloque_apuntador_nivel3;
                super.s_free_blocks_count--;
            }
            
            escribirBloqueApuntadores(archivo, super, bloque_apuntador_nivel2, punteros_nivel2);
            marcarBloqueUsado(archivo, super, bloque_apuntador_nivel2);
            punteros_nivel1.b_pointers[i] = bloque_apuntador_nivel2;
            super.s_free_blocks_count--;
        }
        
        escribirBloqueApuntadores(archivo, super, bloque_apuntador_nivel1, punteros_nivel1);
        marcarBloqueUsado(archivo, super, bloque_apuntador_nivel1);
        inodo.i_block[14] = bloque_apuntador_nivel1;
        super.s_free_blocks_count--;
    }
    
    return true;
}

inline std::string comandoMkfile(const std::string& ruta, int size, 
                                  const std::string& ruta_contenido, bool recursivo) {
    // Validar sesión activa
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }
    
    // Validar que la ruta no esté vacía
    if (ruta.empty() || ruta[0] != '/') {
        return "Error: La ruta debe comenzar con /";
    }
    
    // Obtener partición montada
    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;
    
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }
    
    // Abrir archivo del disco
    FILE* archivo = fopen(particion.path.c_str(), "r+b");
    if (!archivo) {
        return "Error: No se pudo abrir el disco";
    }
    
    // Leer superblock
    Superblock super;
    fseek(archivo, particion.start, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);
    
    // Obtener ruta padre y nombre del archivo
    std::string ruta_padre = obtenerRutaPadre(ruta);
    std::string nombre_archivo = obtenerNombreArchivo(ruta);
    
    // Validar que el nombre no exceda 12 caracteres
    if (nombre_archivo.length() > 12) {
        fclose(archivo);
        return "Error: El nombre del archivo no puede exceder 12 caracteres";
    }
    
    // Si recursivo, crear carpetas padre si no existen
    if (recursivo && ruta_padre != "/") {
        std::vector<std::string> componentes = dividirRuta(ruta_padre);
        std::string ruta_actual = "";
        
        for (size_t i = 0; i < componentes.size(); i++) {
            ruta_actual += "/" + componentes[i];
            
            ResultadoNavegacion resultado = navegarRuta(archivo, super, ruta_actual, false);
            
            if (!resultado.exito) {
                std::string padre_temp = obtenerRutaPadre(ruta_actual);
                std::string nombre_temp = obtenerNombreArchivo(ruta_actual);
                
                ResultadoNavegacion padre_nav = navegarRuta(archivo, super, padre_temp, false);
                if (!padre_nav.exito) {
                    fclose(archivo);
                    return "Error: No se pudo crear la estructura de carpetas";
                }
                
                if (!crearCarpeta(archivo, super, padre_nav.inodo_actual, nombre_temp, 
                                  SessionManager::currentSession.uid, 
                                  SessionManager::currentSession.gid)) {
                    fclose(archivo);
                    return "Error: No se pudo crear la carpeta '" + nombre_temp + "'";
                }
            }
        }
    }
    
    // Navegar al directorio padre
    ResultadoNavegacion padre = navegarRuta(archivo, super, ruta_padre, false);
    if (!padre.exito) {
        fclose(archivo);
        return "Error: La ruta padre no existe. Use -r para crear carpetas intermedias";
    }
    
    // Validar permisos de escritura en la carpeta padre
    Inode inodo_padre = leerInodo(archivo, super, padre.inodo_actual);
    if (!verificarPermiso(inodo_padre, SessionManager::currentSession.uid,
                          SessionManager::currentSession.gid,
                          SessionManager::currentSession.isRoot,
                          PERMISO_ESCRITURA)) {
        fclose(archivo);
        return "Error: No tiene permisos de escritura en la carpeta padre";
    }
    
    // Verificar que el archivo no exista
    ResultadoNavegacion existe = navegarRuta(archivo, super, ruta, false);
    if (existe.exito) {
        fclose(archivo);
        return "Error: El archivo ya existe";
    }
    
    // Determinar el contenido del archivo
    std::string contenido;
    int tamanio_final;
    
    if (!ruta_contenido.empty()) {
        // Leer desde archivo real
        contenido = leerArchivoReal(ruta_contenido);
        if (contenido.empty()) {
            fclose(archivo);
            return "Error: No se pudo leer el archivo: " + ruta_contenido;
        }
        tamanio_final = contenido.length();
    } else if (size > 0) {
        // Generar contenido con patrón
        contenido = generarContenido(size);
        tamanio_final = size;
    } else {
        fclose(archivo);
        return "Error: Debe especificar -size o -cont";
    }
    
    // Buscar inodo libre
    int nuevo_inodo = encontrarInodoLibre(archivo, super);
    if (nuevo_inodo == -1) {
        fclose(archivo);
        return "Error: No hay inodos libres";
    }
    
    // Crear inodo del archivo
    Inode inodo_archivo;
    inodo_archivo.i_uid = SessionManager::currentSession.uid;
    inodo_archivo.i_gid = SessionManager::currentSession.gid;
    inodo_archivo.i_size = tamanio_final;
    inodo_archivo.i_atime = std::time(nullptr);
    inodo_archivo.i_ctime = std::time(nullptr);
    inodo_archivo.i_mtime = std::time(nullptr);
    inodo_archivo.i_type = '0';  // Es archivo
    inodo_archivo.i_perm = 664;
    
    // Inicializar bloques
    for (int i = 0; i < 15; i++) {
        inodo_archivo.i_block[i] = -1;
    }
    
    // Escribir contenido usando apuntadores
    if (!escribirContenidoArchivo(archivo, super, inodo_archivo, contenido)) {
        fclose(archivo);
        return "Error: No se pudo escribir el contenido del archivo";
    }
    
    // Escribir el inodo
    escribirInodo(archivo, super, nuevo_inodo, inodo_archivo);
    marcarInodoUsado(archivo, super, nuevo_inodo);
    super.s_free_inodes_count--;
    
    // Agregar entrada en la carpeta padre
    Inode inodo_padre_obj = leerInodo(archivo, super, padre.inodo_actual);
    bool entrada_agregada = false;
    
    for (int i = 0; i < 12 && inodo_padre_obj.i_block[i] != -1; i++) {
        if (agregarEntradaEnBloque(archivo, super, inodo_padre_obj.i_block[i], 
                                    nombre_archivo, nuevo_inodo)) {
            entrada_agregada = true;
            break;
        }
    }
    
    if (!entrada_agregada) {
        // Necesitamos nuevo bloque en el padre
        int slot_libre = -1;
        for (int i = 0; i < 12; i++) {
            if (inodo_padre_obj.i_block[i] == -1) {
                slot_libre = i;
                break;
            }
        }
        
        if (slot_libre != -1) {
            int bloque_nuevo = encontrarBloqueLibre(archivo, super);
            if (bloque_nuevo != -1) {
                FolderBlock bloque_vacio;
                for (int i = 0; i < 4; i++) {
                    bloque_vacio.b_content[i].b_inodo = -1;
                }
                
                std::strncpy(bloque_vacio.b_content[0].b_name, nombre_archivo.c_str(), 12);
                bloque_vacio.b_content[0].b_inodo = nuevo_inodo;
                
                escribirBloqueCarpeta(archivo, super, bloque_nuevo, bloque_vacio);
                marcarBloqueUsado(archivo, super, bloque_nuevo);
                
                inodo_padre_obj.i_block[slot_libre] = bloque_nuevo;
                escribirInodo(archivo, super, padre.inodo_actual, inodo_padre_obj);
                
                super.s_free_blocks_count--;
            }
        }
    }
    
    // Actualizar superblock
    actualizarSuperblock(archivo, particion.start, super);
    fclose(archivo);
    
    return "Archivo creado exitosamente: " + ruta + " (" + std::to_string(tamanio_final) + " bytes)";
}

#endif