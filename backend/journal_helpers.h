#ifndef JOURNAL_HELPERS_H
#define JOURNAL_HELPERS_H

#include <string>
#include <cstring>
#include <ctime>
#include "structures.h"
#include "mount.h"

namespace JournalHelpers {

    // Verifica si la partición montada es EXT3
    inline bool esExt3(const std::string& id) {
        CommandMount::MountedPartition particion;
        if (!CommandMount::getMountedPartition(id, particion)) return false;

        FILE* archivo = fopen(particion.path.c_str(), "rb");
        if (!archivo) return false;

        Superblock super;
        fseek(archivo, particion.start, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);
        fclose(archivo);

        return super.s_filesystem_type == 3;
    }

    // Lee el Journal completo desde disco
    inline bool leerJournal(FILE* archivo, const Superblock& super, Journal& journal) {
        if (super.s_journal_start <= 0) return false;

        fseek(archivo, super.s_journal_start, SEEK_SET);
        fread(&journal, sizeof(Journal), 1, archivo);
        return true;
    }

    // Escribe el Journal completo en disco
    inline bool escribirJournal(FILE* archivo, const Superblock& super, const Journal& journal) {
        if (super.s_journal_start <= 0) return false;

        fseek(archivo, super.s_journal_start, SEEK_SET);
        fwrite(&journal, sizeof(Journal), 1, archivo);
        return true;
    }

    // Registra una operación en el journal de la partición indicada
    inline void registrar(const std::string& id,
                          const std::string& operacion,
                          const std::string& ruta,
                          const std::string& contenido) {
        CommandMount::MountedPartition particion;
        if (!CommandMount::getMountedPartition(id, particion)) return;

        FILE* archivo = fopen(particion.path.c_str(), "r+b");
        if (!archivo) return;

        Superblock super;
        fseek(archivo, particion.start, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);

        // Solo EXT3
        if (super.s_filesystem_type != 3 || super.s_journal_start <= 0) {
            fclose(archivo);
            return;
        }

        Journal journal;
        leerJournal(archivo, super, journal);

        // Índice con rotación circular: máximo 50 entradas
        int idx = journal.j_count % 50;

        Information& entrada = journal.j_content[idx];

        strncpy(entrada.i_operation, operacion.c_str(), 9);
        strncpy(entrada.i_path,      ruta.c_str(),       31);
        strncpy(entrada.i_content,   contenido.c_str(),  63);

        // Guardar fecha como segundos desde epoch en el float
        entrada.i_date = (float)std::time(nullptr);

        journal.j_count++;

        escribirJournal(archivo, super, journal);
        fclose(archivo);
    }

    // Formatea i_date (float YYYYMMDD.HHMM) a string legible
    inline std::string formatearFecha(float fecha) {
        std::time_t t = (std::time_t)fecha;
        std::tm* tm = std::localtime(&t);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", tm);
        return std::string(buf);
    }

    // Retorna el contenido del journal como texto para el comando journaling
    inline std::string mostrarJournal(const std::string& id) {
        CommandMount::MountedPartition particion;
        if (!CommandMount::getMountedPartition(id, particion)) {
            return "Error: No se encontró la partición con ID '" + id + "'";
        }

        FILE* archivo = fopen(particion.path.c_str(), "rb");
        if (!archivo) {
            return "Error: No se pudo abrir el disco";
        }

        Superblock super;
        fseek(archivo, particion.start, SEEK_SET);
        fread(&super, sizeof(Superblock), 1, archivo);

        if (super.s_filesystem_type != 3) {
            fclose(archivo);
            return "Error: La partición '" + id + "' no es EXT3, no tiene journaling";
        }

        if (super.s_journal_start <= 0) {
            fclose(archivo);
            return "Error: El journal no está inicializado en esta partición";
        }

        Journal journal;
        if (!leerJournal(archivo, super, journal)) {
            fclose(archivo);
            return "Error: No se pudo leer el journal";
        }
        fclose(archivo);

        if (journal.j_count == 0) {
            return "Sin transacciones registradas en la partición '" + id + "'";
        }

        // Calcular cuántas entradas mostrar (máximo 50 por rotación)
        int total = (journal.j_count < 50) ? journal.j_count : 50;

        // Si hubo rotación, el orden cronológico empieza en j_count % 50
        int inicio = (journal.j_count >= 50) ? (journal.j_count % 50) : 0;

        const size_t wOp      = 12;
        const size_t wPath    = 28;
        const size_t wContent = 30;
        const size_t wDate    = 16;

        auto fit = [](const std::string& s, size_t w) -> std::string {
            if (s.size() <= w) return s + std::string(w - s.size(), ' ');
            if (w <= 3)        return s.substr(0, w);
            return s.substr(0, w - 3) + "...";
        };

        auto sep = [&]() -> std::string {
            return "+" + std::string(wOp + 2, '-')
                 + "+" + std::string(wPath + 2, '-')
                 + "+" + std::string(wContent + 2, '-')
                 + "+" + std::string(wDate + 2, '-') + "+\n";
        };

        std::string out = "\n=== JOURNALING ===\n";
        out += "Partición: " + id + "\n";
        out += sep();
        out += "| " + fit("Operacion", wOp)
             + " | " + fit("Path", wPath)
             + " | " + fit("Contenido", wContent)
             + " | " + fit("Fecha", wDate) + " |\n";
        out += sep();

        for (int i = 0; i < total; i++) {
            int idx = (inicio + i) % 50;
            const Information& e = journal.j_content[idx];

            std::string op(e.i_operation);
            std::string path(e.i_path);
            std::string cont(e.i_content);
            std::string fecha = formatearFecha(e.i_date);

            out += "| " + fit(op,    wOp)
                 + " | " + fit(path,  wPath)
                 + " | " + fit(cont,  wContent)
                 + " | " + fit(fecha, wDate) + " |\n";
        }

        out += sep();
        out += "Total: " + std::to_string(journal.j_count) + " transaccion(es) registrada(s)\n";
        return out;
    }

}

#endif