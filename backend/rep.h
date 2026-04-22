#ifndef REP_H
#define REP_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <sys/stat.h>
#include <libgen.h>
#include "structures.h"
#include "mount.h"
#include "filesystem_helpers.h"

namespace CommandRep {
    
    inline std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    inline std::string expandPath(const std::string& path) {
        if (path.empty() || path[0] != '~') {
            return path;
        }
        const char* home = std::getenv("HOME");
        if (!home) {
            std::cerr << "Error: No se pudo obtener el directorio HOME" << std::endl;
            return path;
        }
        return std::string(home) + path.substr(1);
    }
    
    // Crear directorios recursivamente si no existen
    inline bool createDirectories(const std::string& path) {
        char tmp[1024];
        char *p = nullptr;
        size_t len;
        
        snprintf(tmp, sizeof(tmp), "%s", path.c_str());
        len = strlen(tmp);
        if (tmp[len - 1] == '/') {
            tmp[len - 1] = 0;
        }
        
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = 0;
                mkdir(tmp, S_IRWXU);
                *p = '/';
            }
        }
        mkdir(tmp, S_IRWXU);
        return true;
    }
    
    // Obtener el directorio padre de una ruta
    inline std::string getParentPath(const std::string& path) {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s", path.c_str());
        return std::string(dirname(tmp));
    }
    
    // Obtener extensión del archivo
    inline std::string getExtension(const std::string& path) {
        size_t pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            return toLowerCase(path.substr(pos + 1));
        }
        return "jpg"; // Por defecto
    }
    
    inline std::string reportMBR(const std::string& path, const std::string& diskPath) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer MBR
        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        // Generar el DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph MBR_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=TB;\n\n";
        
        // Iniciar tabla única
        dot << "    mbr [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        
        // Cabecera MBR
        dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#000000\"><FONT COLOR=\"white\"><B>MBR</B></FONT></TD></TR>\n";
        dot << "        <TR><TD><B>mbr_tamano</B></TD><TD>" << mbr.mbr_size << "</TD></TR>\n";
        
        // Formatear fecha sin salto de línea
        char dateStr[100];
        struct tm* timeinfo = localtime(&mbr.mbr_creation_date);
        strftime(dateStr, sizeof(dateStr), "%d/%m/%Y %H:%M:%S", timeinfo);
        dot << "        <TR><TD><B>mbr_fecha_creacion</B></TD><TD>" << dateStr << "</TD></TR>\n";
        dot << "        <TR><TD><B>mbr_dsk_signature</B></TD><TD>" << mbr.mbr_disk_signature << "</TD></TR>\n";
        dot << "        <TR><TD><B>dsk_fit</B></TD><TD>" << mbr.disk_fit << "</TD></TR>\n";
        
        // Agregar solo las particiones que existen (status='1')
        int partNum = 1;
        for (int i = 0; i < 4; i++) {
            Partition& part = mbr.mbr_partitions[i];
            
            // Solo mostrar particiones activas
            if (part.part_status == '1') {
                // Cabecera de partición
                dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#000000\"><FONT COLOR=\"white\"><B>Partition " << partNum << "</B></FONT></TD></TR>\n";
                dot << "        <TR><TD><B>status</B></TD><TD>" << part.part_status << "</TD></TR>\n";
                dot << "        <TR><TD><B>type</B></TD><TD>" << part.part_type << "</TD></TR>\n";
                dot << "        <TR><TD><B>fit</B></TD><TD>" << part.part_fit << "</TD></TR>\n";
                dot << "        <TR><TD><B>start</B></TD><TD>" << part.part_start << "</TD></TR>\n";
                dot << "        <TR><TD><B>size</B></TD><TD>" << part.part_size << "</TD></TR>\n";
                dot << "        <TR><TD><B>name</B></TD><TD>" << part.part_name << "</TD></TR>\n";
                partNum++;
            }
        }
        
        dot << "    </TABLE>>];\n\n";
        dot << "}\n";
        
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz para generar la imagen
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\"";
        int result = system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        if (result != 0) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        
        return "Reporte MBR generado exitosamente en: " + path;
    }
    
    inline std::string reportDISK(const std::string& path, const std::string& diskPath) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer MBR
        MBR mbr;
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&mbr), sizeof(MBR));
        
        int diskSize = mbr.mbr_size;
        
        // Estructura para secciones del disco
        struct DiskSection {
            std::string type;
            std::string name;
            int start;
            int size;
            double percent;
            bool isExtended;
        };
        
        std::vector<DiskSection> allSections;
        
        // Agregar MBR
        DiskSection mbrSec;
        mbrSec.type = "mbr";
        mbrSec.name = "MBR";
        mbrSec.start = 0;
        mbrSec.size = sizeof(MBR);
        mbrSec.percent = (sizeof(MBR) * 100.0) / diskSize;
        mbrSec.isExtended = false;
        allSections.push_back(mbrSec);
        
        // Procesar particiones
        for (int i = 0; i < 4; i++) {
            Partition& part = mbr.mbr_partitions[i];
            if (part.part_status == '1') {
                if (part.part_type == 'E' || part.part_type == 'e') {
                    // Partición extendida - expandir con EBR y lógicas
                    int ebr_start = part.part_start;
                    int ext_end = part.part_start + part.part_size;
                    int current_pos = part.part_start;
                    
                    while (ebr_start != -1 && ebr_start < ext_end) {
                        EBR ebr;
                        file.seekg(ebr_start, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                        
                        // Espacio libre antes del EBR
                        if (ebr_start > current_pos) {
                            DiskSection freeSec;
                            freeSec.type = "free";
                            freeSec.name = "Libre";
                            freeSec.start = current_pos;
                            freeSec.size = ebr_start - current_pos;
                            freeSec.percent = (freeSec.size * 100.0) / diskSize;
                            freeSec.isExtended = true;
                            allSections.push_back(freeSec);
                        }
                        
                        if (ebr.part_status == '1') {
                            // EBR
                            DiskSection ebrSec;
                            ebrSec.type = "ebr";
                            ebrSec.name = "EBR";
                            ebrSec.start = ebr_start;
                            ebrSec.size = sizeof(EBR);
                            ebrSec.percent = (sizeof(EBR) * 100.0) / diskSize;
                            ebrSec.isExtended = true;
                            allSections.push_back(ebrSec);
                            
                            // Partición Lógica
                            DiskSection logSec;
                            logSec.type = "logical";
                            logSec.name = std::string(ebr.part_name);
                            logSec.start = ebr_start + sizeof(EBR);
                            logSec.size = ebr.part_size;
                            logSec.percent = (ebr.part_size * 100.0) / diskSize;
                            logSec.isExtended = true;
                            allSections.push_back(logSec);
                            
                            current_pos = ebr_start + sizeof(EBR) + ebr.part_size;
                        }
                        
                        ebr_start = ebr.part_next;
                        if (ebr_start <= 0) break;
                    }
                    
                    // Espacio libre al final de la extendida
                    if (current_pos < ext_end) {
                        DiskSection freeSec;
                        freeSec.type = "free";
                        freeSec.name = "Libre";
                        freeSec.start = current_pos;
                        freeSec.size = ext_end - current_pos;
                        freeSec.percent = (freeSec.size * 100.0) / diskSize;
                        freeSec.isExtended = true;
                        allSections.push_back(freeSec);
                    }
                } else {
                    // Partición primaria
                    DiskSection primSec;
                    primSec.type = "partition";
                    primSec.name = std::string(part.part_name);
                    primSec.start = part.part_start;
                    primSec.size = part.part_size;
                    primSec.percent = (part.part_size * 100.0) / diskSize;
                    primSec.isExtended = false;
                    allSections.push_back(primSec);
                }
            }
        }
        
        // Ordenar por posición
        std::sort(allSections.begin(), allSections.end(), 
                  [](const DiskSection& a, const DiskSection& b) { return a.start < b.start; });
        
        // Calcular espacios libres entre secciones (fuera de extendida)
        std::vector<DiskSection> finalSections;
        int currentPos = 0;
        
        for (const auto& sec : allSections) {
            if (!sec.isExtended && sec.start > currentPos && sec.type != "mbr") {
                DiskSection freeSec;
                freeSec.type = "free";
                freeSec.name = "Libre";
                freeSec.start = currentPos;
                freeSec.size = sec.start - currentPos;
                freeSec.percent = (freeSec.size * 100.0) / diskSize;
                freeSec.isExtended = false;
                finalSections.push_back(freeSec);
            }
            
            finalSections.push_back(sec);
            
            if (!sec.isExtended) {
                currentPos = sec.start + sec.size;
            }
        }
        
        // Espacio libre al final
        if (currentPos < diskSize) {
            DiskSection freeSec;
            freeSec.type = "free";
            freeSec.name = "Libre";
            freeSec.start = currentPos;
            freeSec.size = diskSize - currentPos;
            freeSec.percent = (freeSec.size * 100.0) / diskSize;
            freeSec.isExtended = false;
            finalSections.push_back(freeSec);
        }
        
        // Generar código DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph DISK_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=LR;\n\n";
        
        // Crear tabla con dos filas: encabezado "Extendida" y contenido
        dot << "    disk [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        
        // Primera fila: encabezados (solo "Extendida" sobre las secciones extendidas)
        dot << "        <TR>\n";
        bool inExtended = false;
        int extendedCols = 0;
        
        // Contar columnas de extendida
        for (const auto& sec : finalSections) {
            if (sec.isExtended) {
                extendedCols++;
            }
        }
        
        for (const auto& sec : finalSections) {
            if (sec.isExtended && !inExtended) {
                // Inicio de sección extendida
                inExtended = true;
                dot << "            <TD COLSPAN=\"" << extendedCols << "\" BGCOLOR=\"#FFFFFF\"><B>Extendida</B></TD>\n";
            } else if (!sec.isExtended && inExtended) {
                inExtended = false;
                dot << "            <TD></TD>\n";
            } else if (!sec.isExtended && !inExtended) {
                dot << "            <TD></TD>\n";
            }
        }
        dot << "        </TR>\n";
        
        // Segunda fila: contenido real
        dot << "        <TR>\n";
        
        for (const auto& sec : finalSections) {
            std::string color;
            
            if (sec.type == "mbr") {
                color = "#CCCCCC";
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "</B></TD>\n";
            } else if (sec.type == "free") {
                color = "#FFFFFF";
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% del disco</B></TD>\n";
            } else if (sec.type == "ebr") {
                color = "#B0BEC5";  // Gris más oscuro para EBR
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>" << sec.name << "</B></TD>\n";
            } else if (sec.type == "logical") {
                color = "#E3F2FD";  // Azul claro para lógicas
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>Lógica<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% Del Disco</B></TD>\n";
            } else if (sec.type == "partition") {
                color = "#C8E6C9";  // Verde claro para primarias
                dot << "            <TD BGCOLOR=\"" << color << "\"><B>Primaria<BR/>"
                    << std::fixed << std::setprecision(0) << sec.percent << "% del disco</B></TD>\n";
            }
        }
        
        dot << "        </TR>\n";
        dot << "    </TABLE>>];\n\n";
        dot << "}\n";
        
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz para generar la imagen
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\"";
        int result = system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        if (result != 0) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        
        return "Reporte DISK generado exitosamente en: " + path;
    }

    inline std::string reportBmInode(const std::string& path, const std::string& diskPath, 
                                      int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        
        // Leer bitmap de inodos
        file.seekg(super.s_bm_inode_start, std::ios::beg);
        std::string bitmap;
        bitmap.resize(super.s_inodes_count);
        file.read(&bitmap[0], super.s_inodes_count);
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Generar archivo de texto
        std::ofstream outputFile(path);
        if (!outputFile.is_open()) {
            return "Error: no se pudo crear el archivo de reporte";
        }
        
        // Escribir bitmap (20 bits por línea)
        for (int i = 0; i < super.s_inodes_count; i++) {
            outputFile << bitmap[i];
            
            // Espacio entre bits
            if ((i + 1) % 20 == 0) {
                outputFile << "\n";
            } else {
                outputFile << " ";
            }
        }
        
        outputFile.close();
        return "Reporte BM_INODE generado exitosamente en: " + path;
    }
    
    inline std::string reportBmBlock(const std::string& path, const std::string& diskPath, 
                                      int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        
        // Leer bitmap de bloques
        file.seekg(super.s_bm_block_start, std::ios::beg);
        std::string bitmap;
        bitmap.resize(super.s_blocks_count);
        file.read(&bitmap[0], super.s_blocks_count);
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Generar archivo de texto
        std::ofstream outputFile(path);
        if (!outputFile.is_open()) {
            return "Error: no se pudo crear el archivo de reporte";
        }
        
        // Escribir bitmap (20 bits por línea)
        for (int i = 0; i < super.s_blocks_count; i++) {
            outputFile << bitmap[i];
            
            // Espacio entre bits
            if ((i + 1) % 20 == 0) {
                outputFile << "\n";
            } else {
                outputFile << " ";
            }
        }
        
        outputFile.close();
        return "Reporte BM_BLOCK generado exitosamente en: " + path;
    }
    
    // Reporte SB - Información del Superblock
    inline std::string reportSb(const std::string& path, const std::string& diskPath, 
                                 int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        file.close();
        
        // Generar DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph SB_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=TB;\n\n";
        
        dot << "    sb [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#000000\"><FONT COLOR=\"white\"><B>SUPERBLOCK</B></FONT></TD></TR>\n";
        
        dot << "        <TR><TD><B>s_filesystem_type</B></TD><TD>" << super.s_filesystem_type << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_inodes_count</B></TD><TD>" << super.s_inodes_count << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_blocks_count</B></TD><TD>" << super.s_blocks_count << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_free_blocks_count</B></TD><TD>" << super.s_free_blocks_count << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_free_inodes_count</B></TD><TD>" << super.s_free_inodes_count << "</TD></TR>\n";
        
        // Formatear fechas
        char mtime[100], umtime[100];
        struct tm* timeinfo = localtime(&super.s_mtime);
        strftime(mtime, sizeof(mtime), "%d/%m/%Y %H:%M:%S", timeinfo);
        timeinfo = localtime(&super.s_umtime);
        strftime(umtime, sizeof(umtime), "%d/%m/%Y %H:%M:%S", timeinfo);
        
        dot << "        <TR><TD><B>s_mtime</B></TD><TD>" << mtime << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_umtime</B></TD><TD>" << umtime << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_mnt_count</B></TD><TD>" << super.s_mnt_count << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_magic</B></TD><TD>0x" << std::hex << super.s_magic << std::dec << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_inode_size</B></TD><TD>" << super.s_inode_size << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_block_size</B></TD><TD>" << super.s_block_size << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_first_ino</B></TD><TD>" << super.s_first_ino << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_first_blo</B></TD><TD>" << super.s_first_blo << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_bm_inode_start</B></TD><TD>" << super.s_bm_inode_start << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_bm_block_start</B></TD><TD>" << super.s_bm_block_start << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_inode_start</B></TD><TD>" << super.s_inode_start << "</TD></TR>\n";
        dot << "        <TR><TD><B>s_block_start</B></TD><TD>" << super.s_block_start << "</TD></TR>\n";
        
        dot << "    </TABLE>>];\n\n";
        dot << "}\n";
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\"";
        int result = system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        if (result != 0) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        
        return "Reporte SB generado exitosamente en: " + path;
    }

    // Muestra todos los inodos utilizados
    inline std::string reportInode(const std::string& path, const std::string& diskPath, 
                                    int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        
        // Leer bitmap de inodos
        file.seekg(super.s_bm_inode_start, std::ios::beg);
        std::string bitmap;
        bitmap.resize(super.s_inodes_count);
        file.read(&bitmap[0], super.s_inodes_count);
        
        // Generar DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph INODE_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=TB;\n\n";
        
        // Recorrer inodos utilizados
        for (int i = 0; i < super.s_inodes_count; i++) {
            if (bitmap[i] == '1') {
                // Leer inodo
                Inode inodo;
                file.seekg(super.s_inode_start + (i * sizeof(Inode)), std::ios::beg);
                file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));
                
                // Formatear fechas
                char atime[100], ctime[100], mtime[100];
                struct tm* timeinfo = localtime(&inodo.i_atime);
                strftime(atime, sizeof(atime), "%d/%m/%Y %H:%M:%S", timeinfo);
                timeinfo = localtime(&inodo.i_ctime);
                strftime(ctime, sizeof(ctime), "%d/%m/%Y %H:%M:%S", timeinfo);
                timeinfo = localtime(&inodo.i_mtime);
                strftime(mtime, sizeof(mtime), "%d/%m/%Y %H:%M:%S", timeinfo);
                
                // Crear nodo
                dot << "    inode" << i << " [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
                dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#4CAF50\"><FONT COLOR=\"white\"><B>INODO " << i << "</B></FONT></TD></TR>\n";
                dot << "        <TR><TD><B>i_uid</B></TD><TD>" << inodo.i_uid << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_gid</B></TD><TD>" << inodo.i_gid << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_size</B></TD><TD>" << inodo.i_size << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_atime</B></TD><TD>" << atime << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_ctime</B></TD><TD>" << ctime << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_mtime</B></TD><TD>" << mtime << "</TD></TR>\n";
                
                // Mostrar bloques (solo los usados)
                for (int j = 0; j < 15; j++) {
                    if (inodo.i_block[j] != -1) {
                        dot << "        <TR><TD><B>i_block[" << j << "]</B></TD><TD>" << inodo.i_block[j] << "</TD></TR>\n";
                    }
                }
                
                dot << "        <TR><TD><B>i_type</B></TD><TD>" << (inodo.i_type == '0' ? "Archivo" : "Carpeta") << "</TD></TR>\n";
                dot << "        <TR><TD><B>i_perm</B></TD><TD>" << inodo.i_perm << "</TD></TR>\n";
                dot << "    </TABLE>>];\n\n";
            }
        }
        
        dot << "}\n";
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\" 2>/dev/null";
        system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        // Verificar si se creó el archivo de salida
        std::ifstream checkFile(path);
        if (!checkFile.good()) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        checkFile.close();
        
        return "Reporte INODE generado exitosamente en: " + path;
    }
    
    inline std::string reportBlock(const std::string& path, const std::string& diskPath, 
                                    int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        
        // Leer bitmap de bloques
        file.seekg(super.s_bm_block_start, std::ios::beg);
        std::string bitmap;
        bitmap.resize(super.s_blocks_count);
        file.read(&bitmap[0], super.s_blocks_count);
        
        // Generar DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph BLOCK_Report {\n";
        dot << "    node [shape=plaintext]\n";
        dot << "    rankdir=TB;\n\n";
        
        // Recorrer bloques utilizados
        for (int i = 0; i < super.s_blocks_count; i++) {
            if (bitmap[i] == '1') {
                // Leer bloque genérico
                char buffer[64];
                file.seekg(super.s_block_start + (i * 64), std::ios::beg);
                file.read(buffer, 64);
                
                PointerBlock* pb = reinterpret_cast<PointerBlock*>(buffer);
                FolderBlock* fb = reinterpret_cast<FolderBlock*>(buffer);
                
                bool esApuntador = false;
                bool esCarpeta = false;
                
                // Verificar si es bloque de apuntadores
                if (pb->b_pointers[0] >= 0 && pb->b_pointers[0] < super.s_blocks_count) {
                    esApuntador = true;
                }
                
                // Verificar si es bloque de carpeta
                if (fb->b_content[0].b_inodo >= 0 && fb->b_content[0].b_inodo < super.s_inodes_count) {
                    esCarpeta = true;
                    esApuntador = false;
                }
                
                if (esApuntador) {
                    // Bloque de apuntadores
                    dot << "    block" << i << " [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
                    dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#FF9800\"><FONT COLOR=\"white\"><B>BLOQUE APUNTADORES " << i << "</B></FONT></TD></TR>\n";

                    for (int j = 0; j < 16; j++) {
                        if (pb->b_pointers[j] != -1) {
                            dot << "        <TR><TD><B>Pointer[" << j << "]</B></TD><TD>" << pb->b_pointers[j] << "</TD></TR>\n";
                        }
                    }
                    dot << "    </TABLE>>];\n\n";
                    
                } else if (esCarpeta) {
                    // Bloque de carpeta
                    dot << "    block" << i << " [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
                    dot << "        <TR><TD COLSPAN=\"2\" BGCOLOR=\"#2196F3\"><FONT COLOR=\"white\"><B>BLOQUE CARPETA " << i << "</B></FONT></TD></TR>\n";
                    
                    for (int j = 0; j < 4; j++) {
                        if (fb->b_content[j].b_inodo != -1) {
                            std::string nombre(fb->b_content[j].b_name);
                            size_t pos_nulo = nombre.find('\0');
                            if (pos_nulo != std::string::npos) {
                                nombre = nombre.substr(0, pos_nulo);
                            }
                            std::string nombre_escapado = "";
                            for (size_t k = 0; k < nombre.length(); k++) {
                                char c = nombre[k];
                                if (c == '<') nombre_escapado += "&lt;";
                                else if (c == '>') nombre_escapado += "&gt;";
                                else if (c == '&') nombre_escapado += "&amp;";
                                else if (c == '"') nombre_escapado += "&quot;";
                                else nombre_escapado += c;
                            }
                            dot << "        <TR><TD><B>" << nombre_escapado << "</B></TD><TD>Inodo: " << fb->b_content[j].b_inodo << "</TD></TR>\n";
                        }
                    }
                    dot << "    </TABLE>>];\n\n";
                    
                } else {
                    // Bloque de archivo
                    dot << "    block" << i << " [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
                    dot << "        <TR><TD BGCOLOR=\"#9C27B0\"><FONT COLOR=\"white\"><B>BLOQUE ARCHIVO " << i << "</B></FONT></TD></TR>\n";
                    
                    // Mostrar primeros 64 caracteres
                    std::string contenido(buffer, 64);
                    
                    // Escapar caracteres especiales para DOT
                    std::string contenido_escapado = "";
                    for (size_t k = 0; k < contenido.length() && k < 64; k++) {
                        char c = contenido[k];
                        if (c == '<') contenido_escapado += "&lt;";
                        else if (c == '>') contenido_escapado += "&gt;";
                        else if (c == '&') contenido_escapado += "&amp;";
                        else if (c == '"') contenido_escapado += "&quot;";
                        else if (c == '\n') contenido_escapado += "\\n";
                        else if (c == '\0') break;
                        else if (isprint(c)) contenido_escapado += c;
                        else contenido_escapado += '.';
                    }
                    
                    dot << "        <TR><TD ALIGN=\"LEFT\">" << contenido_escapado << "</TD></TR>\n";
                    dot << "    </TABLE>>];\n\n";
                }
            }
        }
        
        dot << "}\n";
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\" 2>/dev/null";
        system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        // Verificar si se creó el archivo de salida
        std::ifstream checkFile(path);
        if (!checkFile.good()) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        checkFile.close();
        
        return "Reporte BLOCK generado exitosamente en: " + path;
    }

    // Variables globales para el reporte TREE
    static int contadorNodos = 0;
    
    // Función recursiva para generar el árbol
    inline void generarArbolRecursivo(std::ifstream& file, const Superblock& super, 
                                       int inodo_num, std::ostringstream& dot, 
                                       int nivel = 0) {
        // Leer el inodo
        Inode inodo;
        file.seekg(super.s_inode_start + (inodo_num * sizeof(Inode)), std::ios::beg);
        file.read(reinterpret_cast<char*>(&inodo), sizeof(Inode));
        
        // Crear nodo para este inodo
        std::string nodeId = "inode" + std::to_string(inodo_num);
        
        dot << "    " << nodeId << " [label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        dot << "        <TR><TD BGCOLOR=\"#4CAF50\"><FONT COLOR=\"white\"><B>INODO " << inodo_num << "</B></FONT></TD></TR>\n";
        dot << "        <TR><TD>Tipo: " << (inodo.i_type == '0' ? "Archivo" : "Carpeta") << "</TD></TR>\n";
        dot << "        <TR><TD>Size: " << inodo.i_size << " bytes</TD></TR>\n";
        dot << "    </TABLE>>];\n";
        
        // Si es carpeta, procesar sus bloques
        if (inodo.i_type == '1') {
            for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
                FolderBlock bloque;
                file.seekg(super.s_block_start + (inodo.i_block[i] * 64), std::ios::beg);
                file.read(reinterpret_cast<char*>(&bloque), sizeof(FolderBlock));
                
                // Crear nodo para el bloque
                std::string blockId = "block" + std::to_string(inodo.i_block[i]);
                dot << "    " << blockId << " [label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
                dot << "        <TR><TD BGCOLOR=\"#2196F3\"><FONT COLOR=\"white\"><B>BLOQUE " << inodo.i_block[i] << "</B></FONT></TD></TR>\n";
                dot << "    </TABLE>>];\n";
                
                // Conectar inodo con bloque
                dot << "    " << nodeId << " -> " << blockId << ";\n";
                
                // Procesar contenido del bloque
                for (int j = 0; j < 4; j++) {
                    if (bloque.b_content[j].b_inodo != -1) {
                        std::string nombre(bloque.b_content[j].b_name);
                        size_t pos_nulo = nombre.find('\0');
                        if (pos_nulo != std::string::npos) {
                            nombre = nombre.substr(0, pos_nulo);
                        }
                        
                        // Escapar caracteres
                        std::string nombre_escapado = "";
                        for (size_t k = 0; k < nombre.length(); k++) {
                            char c = nombre[k];
                            if (c == '<') nombre_escapado += "&lt;";
                            else if (c == '>') nombre_escapado += "&gt;";
                            else if (c == '&') nombre_escapado += "&amp;";
                            else if (c == '"') nombre_escapado += "&quot;";
                            else nombre_escapado += c;
                        }
                        
                        // Saltar . y ..
                        if (nombre == "." || nombre == "..") {
                            continue;
                        }
                        
                        // Conectar bloque con siguiente inodo
                        std::string siguienteId = "inode" + std::to_string(bloque.b_content[j].b_inodo);
                        dot << "    " << blockId << " -> " << siguienteId << " [label=\"" << nombre_escapado << "\"];\n";
                        
                        // Llamada recursiva
                        generarArbolRecursivo(file, super, bloque.b_content[j].b_inodo, dot, nivel + 1);
                    }
                }
            }
        } else {
            // Es archivo, mostrar sus bloques de contenido
            for (int i = 0; i < 12 && inodo.i_block[i] != -1; i++) {
                std::string blockId = "fileblock" + std::to_string(inodo.i_block[i]);
                dot << "    " << blockId << " [label=\"BLOQUE " << inodo.i_block[i] << "\" shape=box style=filled fillcolor=\"#9C27B0\" fontcolor=white];\n";
                dot << "    " << nodeId << " -> " << blockId << ";\n";
            }
        }
    }
    
    inline std::string reportTree(const std::string& path, const std::string& diskPath, 
                                   int inicio_particion) {
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }
        
        // Leer Superblock
        Superblock super;
        file.seekg(inicio_particion, std::ios::beg);
        file.read(reinterpret_cast<char*>(&super), sizeof(Superblock));
        
        // Generar DOT para Graphviz
        std::ostringstream dot;
        dot << "digraph TREE_Report {\n";
        dot << "    rankdir=TB;\n";
        dot << "    node [shape=plaintext];\n\n";
        
        // Generar árbol desde la raíz (inodo 0)
        contadorNodos = 0;
        generarArbolRecursivo(file, super, 0, dot, 0);
        
        dot << "}\n";
        file.close();
        
        // Crear directorio si no existe
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        // Guardar archivo .dot
        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();
        
        // Ejecutar Graphviz
        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\" 2>/dev/null";
        system(cmd.c_str());
        
        // Eliminar archivo .dot temporal
        remove(dotPath.c_str());
        
        // Verificar si se creó el archivo
        std::ifstream checkFile(path);
        if (!checkFile.good()) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        checkFile.close();
        
        return "Reporte TREE generado exitosamente en: " + path;
    }
    
    // Reporte FILE - Contenido de un archivo específico
    inline std::string reportFile(const std::string& path, const std::string& diskPath, 
                                   int inicio_particion, const std::string& rutaArchivo) {
        if (rutaArchivo.empty()) {
            return "Error: debe especificar -path_file_ls con la ruta del archivo";
        }
        
        FILE* archivo = fopen(diskPath.c_str(), "rb");
        if (!archivo) {
            return "Error: no se pudo abrir el disco";
        }
        
        // Leer Superblock
        Superblock super;
        fseek(archivo, inicio_particion, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);
        fclose(archivo);
        
        // Crear archivo de texto con el contenido
        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);
        
        std::ofstream outputFile(path);
        if (!outputFile.is_open()) {
            return "Error: no se pudo crear el archivo de reporte";
        }
        
        outputFile << "=== Contenido del archivo: " << rutaArchivo << " ===\n";
        outputFile << "(Implementación pendiente - requiere integración con cat.h)\n";
        outputFile.close();
        
        return "Reporte FILE generado en: " + path + " (implementación básica)";
    }
    
    // Reporte LS como imagen Graphviz (mantener del Proyecto 1)
    inline std::string reportLsGrafico(const std::string& path, const std::string& diskPath, 
                                        int inicio_particion, const std::string& rutaCarpeta) {
        std::string ruta = rutaCarpeta.empty() ? "/" : rutaCarpeta;
                                        
        FILE* archivo = fopen(diskPath.c_str(), "rb");
        if (!archivo) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }

        Superblock super;
        fseek(archivo, inicio_particion, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);

        // Navegar a la ruta especificada
        ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
        if (!nav.exito) {
            fclose(archivo);
            return "Error: no se encontró la ruta '" + ruta + "'";
        }

        Inode inodo_dir = leerInodo(archivo, super, nav.inodo_actual);
        if (inodo_dir.i_type != '1') {
            fclose(archivo);
            return "Error: la ruta '" + ruta + "' no es un directorio";
        }

        std::ostringstream dot;
        dot << "digraph LS_Report {\n";
        dot << "    node [shape=plaintext];\n";
        dot << "    rankdir=TB;\n\n";

        dot << "    ls [label=<<TABLE BORDER=\"1\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
        dot << "        <TR BGCOLOR=\"#000000\">\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Permisos</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Owner</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Grupo</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Size</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Fecha</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Tipo</B></FONT></TD>\n";
        dot << "            <TD><FONT COLOR=\"white\"><B>Name</B></FONT></TD>\n";
        dot << "        </TR>\n";

        for (int i = 0; i < 12 && inodo_dir.i_block[i] != -1; i++) {
            FolderBlock bloque;
            int posBloque = super.s_block_start + (inodo_dir.i_block[i] * 64);
            fseek(archivo, posBloque, SEEK_SET);
            fread(&bloque, sizeof(FolderBlock), 1, archivo);

            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == -1) continue;

                char buf[13] = {0};
                strncpy(buf, bloque.b_content[j].b_name, 12);
                std::string nombre(buf);
                if (nombre == "." || nombre == "..") continue;

                int inodo_idx = bloque.b_content[j].b_inodo;
                if (inodo_idx < 0 || inodo_idx >= super.s_inodes_count) continue;

                Inode inodo_hijo = leerInodo(archivo, super, inodo_idx);

                std::string permisos = std::to_string(inodo_hijo.i_perm);
                char fecha[32];
                struct tm* timeinfo = localtime(&inodo_hijo.i_mtime);
                strftime(fecha, sizeof(fecha), "%d/%m/%Y %H:%M", timeinfo);
                std::string tipo = (inodo_hijo.i_type == '0') ? "Archivo" : "Carpeta";

                dot << "        <TR>\n";
                dot << "            <TD>" << permisos << "</TD>\n";
                dot << "            <TD>" << inodo_hijo.i_uid << "</TD>\n";
                dot << "            <TD>" << inodo_hijo.i_gid << "</TD>\n";
                dot << "            <TD>" << inodo_hijo.i_size << "</TD>\n";
                dot << "            <TD>" << fecha << "</TD>\n";
                dot << "            <TD>" << tipo << "</TD>\n";
                dot << "            <TD>" << nombre << "</TD>\n";
                dot << "        </TR>\n";
            }
        }

        dot << "    </TABLE>>];\n";
        dot << "}\n";
        fclose(archivo);

        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);

        std::string dotPath = path + ".dot";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            return "Error: no se pudo crear el archivo .dot";
        }
        dotFile << dot.str();
        dotFile.close();

        std::string ext = getExtension(path);
        std::string cmd = "dot -T" + ext + " \"" + dotPath + "\" -o \"" + path + "\" 2>/dev/null";
        system(cmd.c_str());
        remove(dotPath.c_str());

        std::ifstream checkFile(path);
        if (!checkFile.good()) {
            return "Error: no se pudo generar el reporte con Graphviz";
        }
        checkFile.close();

        return "Reporte LS generado exitosamente en: " + path;
    }

    // Reporte LS como texto plano (para el visualizador web)
    inline std::string reportLs(const std::string& path, const std::string& diskPath, 
                                 int inicio_particion, const std::string& rutaCarpeta) {
        std::string ruta = rutaCarpeta.empty() ? "/" : rutaCarpeta;
                                
        // Si la extensión es jpg/png/svg, generar gráfico
        std::string ext = getExtension(path);
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "svg" || ext == "pdf") {
            return reportLsGrafico(path, diskPath, inicio_particion, rutaCarpeta);
        }

        // Si es .txt, generar texto plano para el visualizador web
        FILE* archivo = fopen(diskPath.c_str(), "rb");
        if (!archivo) {
            return "Error: no se pudo abrir el disco '" + diskPath + "'";
        }

        Superblock super;
        fseek(archivo, inicio_particion, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);

        ResultadoNavegacion nav = navegarRuta(archivo, super, ruta, false);
        if (!nav.exito) {
            fclose(archivo);
            return "Error: no se encontró la ruta '" + ruta + "'";
        }

        Inode inodo_dir = leerInodo(archivo, super, nav.inodo_actual);
        if (inodo_dir.i_type != '1') {
            fclose(archivo);
            return "Error: la ruta '" + ruta + "' no es un directorio";
        }

        std::string parentPath = getParentPath(path);
        createDirectories(parentPath);

        std::ofstream outputFile(path);
        if (!outputFile.is_open()) {
            fclose(archivo);
            return "Error: no se pudo crear el archivo de reporte";
        }

        outputFile << "Permisos | Owner | Grupo | Size | Fecha | Tipo | Nombre\n";

        for (int i = 0; i < 12 && inodo_dir.i_block[i] != -1; i++) {
            FolderBlock bloque;
            int posBloque = super.s_block_start + (inodo_dir.i_block[i] * 64);
            fseek(archivo, posBloque, SEEK_SET);
            fread(&bloque, sizeof(FolderBlock), 1, archivo);
        
            for (int j = 0; j < 4; j++) {
                if (bloque.b_content[j].b_inodo == -1) continue;

                char buf[13] = {0};
                strncpy(buf, bloque.b_content[j].b_name, 12);
                std::string nombre(buf);
                if (nombre == "." || nombre == "..") continue;

                int inodo_idx = bloque.b_content[j].b_inodo;
                if (inodo_idx < 0 || inodo_idx >= super.s_inodes_count) continue;

                Inode inodo_hijo = leerInodo(archivo, super, inodo_idx);

                std::string permisos = std::to_string(inodo_hijo.i_perm);
                char fecha[32];
                struct tm* timeinfo = localtime(&inodo_hijo.i_mtime);
                strftime(fecha, sizeof(fecha), "%d/%m/%Y %H:%M", timeinfo);
                std::string tipo = (inodo_hijo.i_type == '0') ? "Archivo" : "Carpeta";
            
                outputFile << permisos << " | "
                           << inodo_hijo.i_uid << " | "
                           << inodo_hijo.i_gid << " | "
                           << inodo_hijo.i_size << " | "
                           << fecha << " | "
                           << tipo << " | "
                           << nombre << "\n";
            }
        }

        outputFile.close();
        fclose(archivo);

        return "Reporte LS generado exitosamente en: " + path;
    }
    
    // Función principal del comando REP
    inline std::string execute(const std::string& name, const std::string& path, 
                               const std::string& id, const std::string& pathFileLs) {
        // Validar parámetros obligatorios
        if (name.empty()) {
            return "Error: rep requiere el parámetro -name";
        }
        if (path.empty()) {
            return "Error: rep requiere el parámetro -path";
        }
        if (id.empty()) {
            return "Error: rep requiere el parámetro -id";
        }
        
        std::string expandedPath = expandPath(path);
        std::string reportType = toLowerCase(name);
        
        // Validar tipo de reporte
        if (reportType != "mbr" && reportType != "disk" && reportType != "inode" && 
            reportType != "block" && reportType != "bm_inode" && reportType != "bm_block" && 
            reportType != "tree" && reportType != "sb" && reportType != "file" && reportType != "ls") {
            return "Error: tipo de reporte no válido. Valores permitidos: mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file, ls";
        }
        
        // Buscar la partición montada
        CommandMount::MountedPartition partition;
        if (!CommandMount::getMountedPartition(id, partition)) {
            return "Error: la partición con ID '" + id + "' no está montada";
        }
        
        std::ostringstream result;
        result << "\n=== REP ===\n";
        result << "Generando reporte '" << reportType << "'...\n";
        
        // Ejecutar el reporte correspondiente
        if (reportType == "mbr") {
            std::string res = reportMBR(expandedPath, partition.path);
            result << res << "\n";
        } else if (reportType == "disk") {
            std::string res = reportDISK(expandedPath, partition.path);
            result << res << "\n";
        } else if (reportType == "bm_inode") {
            std::string res = reportBmInode(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "bm_block") {
            std::string res = reportBmBlock(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "sb") {
            std::string res = reportSb(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "sb") {
            std::string res = reportSb(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "inode") {
            std::string res = reportInode(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "block") {
            std::string res = reportBlock(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "tree") {
            std::string res = reportTree(expandedPath, partition.path, partition.start);
            result << res << "\n";
        } else if (reportType == "file") {
            std::string res = reportFile(expandedPath, partition.path, partition.start, pathFileLs);
            result << res << "\n";
        } else if (reportType == "ls") {
            std::string res = reportLs(expandedPath, partition.path, partition.start, pathFileLs);
            result << res << "\n";
        } else {
            result << "Reporte '" << reportType << "' aún no implementado\n";
        }
        
        return result.str();
    }
}
#endif // REP_H
