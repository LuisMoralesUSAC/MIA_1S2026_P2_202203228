#ifndef LOGIN_H
#define LOGIN_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"

struct LineaUsuario {
    int id;
    char tipo;
    std::string grupo;
    std::string usuario;
    std::string password;
};

LineaUsuario parsearLineaUsuario(const std::string& linea) {
    LineaUsuario resultado;
    std::stringstream ss(linea);
    std::string token;
    std::vector<std::string> tokens;
    
    while (getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() >= 3) {
        resultado.id = stoi(tokens[0]);
        resultado.tipo = tokens[1][0];
        resultado.grupo = tokens[2];
        
        if (resultado.tipo == 'U' && tokens.size() >= 5) {
            resultado.usuario = tokens[3];
            resultado.password = tokens[4];
        }
    }
    
    return resultado;
}

std::string leerUserstxt(const std::string& ruta_particion, int inicio_particion) {
    FILE* archivo = fopen(ruta_particion.c_str(), "rb");
    if (!archivo) {
        return "";
    }
    Superblock super;
    fseek(archivo, inicio_particion, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);
    
    Inode inodo_users;
    int posicion_inodo1 = super.s_inode_start + (1 * sizeof(Inode));
    fseek(archivo, posicion_inodo1, SEEK_SET);
    fread(&inodo_users, sizeof(Inode), 1, archivo);

    std::string contenido = "";
    
    for (int i = 0; i < 12; i++) {
        if (inodo_users.i_block[i] == -1) break;
        
        FileBlock bloque;
        int posicion_bloque = super.s_block_start + (inodo_users.i_block[i] * sizeof(FileBlock));
        fseek(archivo, posicion_bloque, SEEK_SET);
        fread(&bloque, sizeof(FileBlock), 1, archivo);
        contenido += std::string(bloque.b_content, 64);
    }
    
    fclose(archivo);
    size_t pos_nulo = contenido.find('\0');
    if (pos_nulo != std::string::npos) {
        contenido = contenido.substr(0, pos_nulo);
    }
    
    return contenido;
}

inline bool buscarParticionPorId(const std::string& id, CommandMount::MountedPartition& particion) {
    // Primero intentar con el mapa en memoria
    if (CommandMount::getMountedPartition(id, particion)) {
        return true;
    }
    return false;
}

std::string comandoLogin(const std::string& usuario, const std::string& password, const std::string& id) {
    if (SessionManager::currentSession.isLoggedIn()) {
        return "Error: Ya hay una sesión activa. Use 'logout' primero.";
    }
    
    CommandMount::MountedPartition particion_montada;
    bool encontrada = CommandMount::getMountedPartition(id, particion_montada);
    
    if (!encontrada) {
        return "Error: No se encontró ninguna partición montada con el ID '" + id + "'";
    }
    
    std::string contenido_users = leerUserstxt(particion_montada.path, particion_montada.start);
    
    if (contenido_users.empty()) {
        return "Error: No se pudo leer el archivo users.txt. ¿La partición está formateada?";
    }
    
    std::stringstream ss(contenido_users);
    std::string linea;
    
    bool usuario_encontrado = false;
    int uid_encontrado = -1;
    int gid_encontrado = -1;
    
    while (getline(ss, linea)) {
        if (linea.empty() || linea[0] == '\n') continue;
        
        LineaUsuario datos = parsearLineaUsuario(linea);
        
        if (datos.tipo == 'U') {
            if (datos.id != 0 && datos.usuario == usuario && datos.password == password) {
                usuario_encontrado = true;
                uid_encontrado = datos.id;
                
                std::stringstream ss2(contenido_users);
                std::string linea2;
                while (getline(ss2, linea2)) {
                    if (linea2.empty()) continue;
                    
                    LineaUsuario datos_grupo = parsearLineaUsuario(linea2);
                    if (datos_grupo.tipo == 'G' && datos_grupo.grupo == datos.grupo) {
                        gid_encontrado = datos_grupo.id;
                        break;
                    }
                }
                
                break;
            }
        }
    }
    
    if (!usuario_encontrado) {
        return "Error: Usuario o contraseña incorrectos";
    }
    
    if (gid_encontrado == -1) {
        return "Error: No se encontró el grupo del usuario";
    }
    
    SessionManager::currentSession.login(usuario, id, uid_encontrado, gid_encontrado);
    
    std::string mensaje = "Sesión iniciada exitosamente\n";
    mensaje += "Usuario: " + usuario + "\n";
    mensaje += "ID Partición: " + id + "\n";
    mensaje += "UID: " + std::to_string(uid_encontrado) + "\n";
    mensaje += "GID: " + std::to_string(gid_encontrado);
    
    return mensaje;
}

#endif