#ifndef CHGRP_H
#define CHGRP_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"

struct RegistroChgrp {
    int id;
    char tipo;
    std::string grupo;
    std::string usuario;
    std::string password;
};

RegistroChgrp parsearRegistroChgrp(const std::string& linea) {
    RegistroChgrp reg;
    std::stringstream ss(linea);
    std::string token;
    std::vector<std::string> tokens;
    
    while (getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() >= 3) {
        reg.id = stoi(tokens[0]);
        reg.tipo = tokens[1][0];
        reg.grupo = tokens[2];
        
        if (reg.tipo == 'U' && tokens.size() >= 5) {
            reg.usuario = tokens[3];
            reg.password = tokens[4];
        }
    }
    
    return reg;
}

std::string leerUsersChgrp(const std::string& ruta_disco, int inicio_particion) {
    FILE* archivo = fopen(ruta_disco.c_str(), "rb");
    if (!archivo) return "";
    
    Superblock super;
    fseek(archivo, inicio_particion, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);
    
    Inode inodo_users;
    int pos_inodo1 = super.s_inode_start + (1 * sizeof(Inode));
    fseek(archivo, pos_inodo1, SEEK_SET);
    fread(&inodo_users, sizeof(Inode), 1, archivo);
    
    std::string contenido = "";
    
    for (int i = 0; i < 12; i++) {
        if (inodo_users.i_block[i] == -1) break;
        
        FileBlock bloque;
        int pos_bloque = super.s_block_start + (inodo_users.i_block[i] * sizeof(FileBlock));
        fseek(archivo, pos_bloque, SEEK_SET);
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

bool escribirUsersChgrp(const std::string& ruta_disco, int inicio_particion, const std::string& contenido) {
    FILE* archivo = fopen(ruta_disco.c_str(), "r+b");
    if (!archivo) return false;
    
    Superblock super;
    fseek(archivo, inicio_particion, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);
    
    Inode inodo_users;
    int pos_inodo1 = super.s_inode_start + (1 * sizeof(Inode));
    fseek(archivo, pos_inodo1, SEEK_SET);
    fread(&inodo_users, sizeof(Inode), 1, archivo);
    
    inodo_users.i_size = contenido.length();
    inodo_users.i_mtime = time(nullptr);
    
    int bloques_necesarios = (contenido.length() + 63) / 64;
    
    for (int i = 0; i < bloques_necesarios && i < 12; i++) {
        if (inodo_users.i_block[i] == -1) {
            fseek(archivo, super.s_bm_block_start, SEEK_SET);
            char bitmap[super.s_blocks_count];
            fread(bitmap, 1, super.s_blocks_count, archivo);
            
            int bloque_libre = -1;
            for (int j = 0; j < super.s_blocks_count; j++) {
                if (bitmap[j] == '0') {
                    bloque_libre = j;
                    break;
                }
            }
            
            if (bloque_libre == -1) {
                fclose(archivo);
                return false;
            }
            
            inodo_users.i_block[i] = bloque_libre;
            bitmap[bloque_libre] = '1';
            fseek(archivo, super.s_bm_block_start, SEEK_SET);
            fwrite(bitmap, 1, super.s_blocks_count, archivo);
            super.s_free_blocks_count--;
        }
    }
    
    int bytes_escritos = 0;
    int bloque_actual = 0;
    
    while (bytes_escritos < (int)contenido.length() && bloque_actual < 12) {
        FileBlock bloque;
        memset(bloque.b_content, 0, 64);
        
        int bytes_a_escribir = std::min(64, (int)contenido.length() - bytes_escritos);
        memcpy(bloque.b_content, contenido.c_str() + bytes_escritos, bytes_a_escribir);
        
        int pos_bloque = super.s_block_start + (inodo_users.i_block[bloque_actual] * sizeof(FileBlock));
        fseek(archivo, pos_bloque, SEEK_SET);
        fwrite(&bloque, sizeof(FileBlock), 1, archivo);
        
        bytes_escritos += bytes_a_escribir;
        bloque_actual++;
    }
    
    fseek(archivo, pos_inodo1, SEEK_SET);
    fwrite(&inodo_users, sizeof(Inode), 1, archivo);
    
    fseek(archivo, inicio_particion, SEEK_SET);
    fwrite(&super, sizeof(Superblock), 1, archivo);
    
    fclose(archivo);
    return true;
}

std::string comandoChgrp(const std::string& usuario, const std::string& grupo) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }
    
    if (!SessionManager::currentSession.isRoot) {
        return "Error: Solo el usuario root puede cambiar grupos";
    }
    
    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;
    
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }
    
    std::string contenido = leerUsersChgrp(particion.path, particion.start);
    if (contenido.empty()) {
        return "Error: No se pudo leer el archivo users.txt";
    }
    
    std::stringstream ss_validar(contenido);
    std::string linea_validar;
    bool grupo_existe = false;
    
    while (getline(ss_validar, linea_validar)) {
        if (linea_validar.empty()) continue;
        
        RegistroChgrp reg = parsearRegistroChgrp(linea_validar);
        if (reg.tipo == 'G' && reg.id != 0 && reg.grupo == grupo) {
            grupo_existe = true;
            break;
        }
    }
    
    if (!grupo_existe) {
        return "Error: El grupo '" + grupo + "' no existe";
    }

    std::stringstream ss(contenido);
    std::string linea;
    std::string nuevo_contenido = "";
    bool usuario_encontrado = false;
    
    while (getline(ss, linea)) {
        if (linea.empty()) continue;
        
        RegistroChgrp reg = parsearRegistroChgrp(linea);
        
        if (reg.tipo == 'U' && reg.id != 0 && reg.usuario == usuario) {
            usuario_encontrado = true;
            nuevo_contenido += std::to_string(reg.id) + ",U," + grupo + "," + reg.usuario + "," + reg.password + "\n";
        } else {
            nuevo_contenido += linea + "\n";
        }
    }
    
    if (!usuario_encontrado) {
        return "Error: El usuario '" + usuario + "' no existe";
    }

    if (!escribirUsersChgrp(particion.path, particion.start, nuevo_contenido)) {
        return "Error: No se pudo actualizar el archivo users.txt";
    }
    
    return "Grupo del usuario '" + usuario + "' cambiado a '" + grupo + "' exitosamente";
}

#endif