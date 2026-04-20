#ifndef LOSS_H
#define LOSS_H

#include <string>
#include <cstring>
#include "structures.h"
#include "mount.h"

inline std::string comandoLoss(const std::string& id) {
    if (id.empty()) {
        return "Error: loss requiere el parámetro -id";
    }

    CommandMount::MountedPartition particion;
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No se encontró la partición con ID '" + id + "'";
    }

    FILE* archivo = fopen(particion.path.c_str(), "r+b");
    if (!archivo) {
        return "Error: No se pudo abrir el disco";
    }

    Superblock super;
    fseek(archivo, particion.start, SEEK_SET);
    fread(&super, sizeof(Superblock), 1, archivo);

    if (super.s_filesystem_type != 3) {
        fclose(archivo);
        return "Error: La partición '" + id + "' no es EXT3, loss solo aplica a EXT3";
    }

    // Buffer de ceros
    const int BUFFER_SIZE = 4096;
    char ceros[BUFFER_SIZE];
    memset(ceros, 0, BUFFER_SIZE);

    auto limpiarZona = [&](int inicio, int tamanio) {
        fseek(archivo, inicio, SEEK_SET);
        int restante = tamanio;
        while (restante > 0) {
            int a_escribir = (restante < BUFFER_SIZE) ? restante : BUFFER_SIZE;
            fwrite(ceros, 1, a_escribir, archivo);
            restante -= a_escribir;
        }
    };

    // Limpiar bitmap de inodos
    limpiarZona(super.s_bm_inode_start, super.s_inodes_count);

    // Limpiar bitmap de bloques
    limpiarZona(super.s_bm_block_start, super.s_blocks_count);

    // Limpiar área de inodos
    int tamanio_inodos = super.s_inodes_count * sizeof(Inode);
    limpiarZona(super.s_inode_start, tamanio_inodos);

    // Limpiar área de bloques (saltando el journal)
    int journal_size = sizeof(Journal);
    int journal_inicio = super.s_journal_start;
    int journal_fin = journal_inicio + journal_size;
    int bloques_inicio = super.s_block_start;
    int bloques_fin = bloques_inicio + (super.s_blocks_count * 64);

    // Limpiar desde inicio de bloques hasta antes del journal
    if (bloques_inicio < journal_inicio) {
        limpiarZona(bloques_inicio, journal_inicio - bloques_inicio);
    }

    // Limpiar desde después del journal hasta fin del área de bloques
    if (journal_fin < bloques_fin) {
        limpiarZona(journal_fin, bloques_fin - journal_fin);
    }

    fclose(archivo);

    return "Pérdida del sistema de archivos simulada exitosamente en partición '" + id + "'\n"
           "  Bitmap de inodos: limpiado\n"
           "  Bitmap de bloques: limpiado\n"
           "  Área de inodos: limpiada\n"
           "  Área de bloques: limpiada\n"
           "  Superblock y Journal: intactos";
}

#endif