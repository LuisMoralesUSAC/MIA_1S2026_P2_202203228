#ifndef MKDIR_H
#define MKDIR_H

#include <iostream>
#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "session.h"
#include "mount.h"
#include "filesystem_helpers.h"

inline std::string comandoMkdir(const std::string& ruta, bool crear_padres) {
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
    
    if (crear_padres) {
        std::vector<std::string> componentes = dividirRuta(ruta);
        std::string ruta_actual = "";
        
        for (size_t i = 0; i < componentes.size(); i++) {
            ruta_actual += "/" + componentes[i];
            
            // Verificar si la carpeta ya existe
            ResultadoNavegacion resultado = navegarRuta(archivo, super, ruta_actual, false);
            
            if (!resultado.exito) {
                // La carpeta no existe, crearla
                std::string ruta_padre = obtenerRutaPadre(ruta_actual);
                std::string nombre = obtenerNombreArchivo(ruta_actual);
                
                // Navegar al padre
                ResultadoNavegacion padre = navegarRuta(archivo, super, ruta_padre, false);
                if (!padre.exito) {
                    fclose(archivo);
                    return "Error: No se pudo acceder a la ruta padre: " + ruta_padre;
                }
                
                // Crear la carpeta
                if (!crearCarpeta(archivo, super, padre.inodo_actual, nombre, 
                                  SessionManager::currentSession.uid, 
                                  SessionManager::currentSession.gid)) {
                    fclose(archivo);
                    return "Error: No se pudo crear la carpeta '" + nombre + "'";
                }
            }
        }
        
        // Actualizar superblock
        actualizarSuperblock(archivo, particion.start, super);
        fclose(archivo);
        
        return "Carpeta(s) creada(s) exitosamente: " + ruta;
        
    } else {
        std::string ruta_padre = obtenerRutaPadre(ruta);
        std::string nombre = obtenerNombreArchivo(ruta);
        
        // Validar que el nombre no exceda 12 caracteres
        if (nombre.length() > 12) {
            fclose(archivo);
            return "Error: El nombre de la carpeta no puede exceder 12 caracteres";
        }
        
        // Navegar al padre
        ResultadoNavegacion padre = navegarRuta(archivo, super, ruta_padre, false);
        if (!padre.exito) {
            fclose(archivo);
            return "Error: La ruta padre no existe. Use -p para crear carpetas intermedias";
        }
        
        // Verificar que la carpeta no exista ya
        ResultadoNavegacion existe = navegarRuta(archivo, super, ruta, false);
        if (existe.exito) {
            fclose(archivo);
            return "Error: La carpeta ya existe";
        }
        
        // Crear la carpeta
        if (!crearCarpeta(archivo, super, padre.inodo_actual, nombre, 
                          SessionManager::currentSession.uid, 
                          SessionManager::currentSession.gid)) {
            fclose(archivo);
            return "Error: No se pudo crear la carpeta";
        }
        
        // Actualizar superblock
        actualizarSuperblock(archivo, particion.start, super);
        fclose(archivo);
        
        return "Carpeta creada exitosamente: " + ruta;
    }
}

#endif