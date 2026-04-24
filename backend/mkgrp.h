#ifndef MKGRP_H
#define MKGRP_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "structures.h"
#include "session.h"
#include "mount.h"

struct RegistroUsuario {
    int id;
    char tipo;  // 'U' o 'G'
    std::string grupo;
    std::string usuario;
    std::string password;
};

RegistroUsuario parsearRegistro(const std::string& linea) {
    RegistroUsuario reg;
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

std::string leerUsersCompleto(const std::string& ruta_disco, int inicio_particion) {
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

bool escribirUsersCompleto(const std::string& ruta_disco, int inicio_particion, const std::string& contenido) {
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

            std::vector<char> bitmap(super.s_blocks_count);
            fseek(archivo, super.s_bm_block_start, SEEK_SET);
            fread(bitmap.data(), 1, super.s_blocks_count, archivo);
            
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
            fwrite(bitmap.data(), 1, super.s_blocks_count, archivo);
            super.s_free_blocks_count--;

            fseek(archivo, inicio_particion, SEEK_SET);
            fwrite(&super, sizeof(Superblock), 1, archivo);
        }
    }

    int bytes_escritos = 0;
    int bloque_actual = 0;

    while (bytes_escritos < (int)contenido.length() && bloque_actual < 12) {
        if (inodo_users.i_block[bloque_actual] == -1) break;
        
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

    fclose(archivo);
    return true;
}

std::string comandoMkgrp(const std::string& nombre_grupo) {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: Debe iniciar sesión primero";
    }
    
    if (!SessionManager::currentSession.isRoot) {
        return "Error: Solo el usuario root puede crear grupos";
    }
    
    CommandMount::MountedPartition particion;
    std::string id = SessionManager::currentSession.currentID;
    
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se pudo acceder a la partición montada";
    }
    
    std::string contenido = leerUsersCompleto(particion.path, particion.start);
    if (contenido.empty()) {
        return "Error: No se pudo leer el archivo users.txt";
    }
    
    std::stringstream ss(contenido);
    std::string linea;
    std::vector<RegistroUsuario> registros;
    int max_gid = 0;
    
    while (getline(ss, linea)) {
        if (linea.empty()) continue;
        
        RegistroUsuario reg = parsearRegistro(linea);
        registros.push_back(reg);
        
        if (reg.tipo == 'G' && reg.id != 0 && reg.grupo == nombre_grupo) {
            return "Error: El grupo '" + nombre_grupo + "' ya existe";
        }
        
        if (reg.tipo == 'G' && reg.id > max_gid) {
            max_gid = reg.id;
        }
    }
    
    int nuevo_gid = max_gid + 1;
    
    std::string nueva_linea = std::to_string(nuevo_gid) + ",G," + nombre_grupo + "\n";
    
    std::string nuevo_contenido = contenido + nueva_linea;
    
    if (!escribirUsersCompleto(particion.path, particion.start, nuevo_contenido)) {
        return "Error: No se pudo actualizar el archivo users.txt";
    }
    
    return "Grupo '" + nombre_grupo + "' creado exitosamente con GID: " + std::to_string(nuevo_gid);
}

#endif