#ifndef UNMOUNT_H
#define UNMOUNT_H

#include <string>
#include "mount.h"
#include "session.h"

inline std::string comandoUnmount(const std::string& id) {
    if (id.empty()) {
        return "Error: El parámetro -id es obligatorio";
    }

    // Verificar que la partición esté montada
    CommandMount::MountedPartition particion;
    if (!CommandMount::getMountedPartition(id, particion)) {
        return "Error: No existe ninguna partición montada con ID '" + id + "'";
    }

    // Verificar que no haya sesión activa en esta partición
    if (SessionManager::currentSession.isLoggedIn() &&
        SessionManager::currentSession.currentID == id) {
        return "Error: No se puede desmontar la partición, hay una sesión activa en ella. "
               "Use 'logout' primero";
    }

    // Guardar info antes de desmontar para el mensaje
    std::string nombre = particion.name;
    std::string disco  = particion.path;

    // Eliminar del mapa de particiones montadas
    CommandMount::mountedPartitions.erase(id);

    // Recalcular el correlativo de partición para ese disco
    bool quedanParticiones = false;
    for (const auto& [mid, mp] : CommandMount::mountedPartitions) {
        if (mp.path == disco) {
            quedanParticiones = true;
            break;
        }
    }

    if (!quedanParticiones) {
        CommandMount::diskLetters.erase(disco);
    }
    
    return "Partición desmontada exitosamente\n"
           "  ID: " + id + "\n"
           "  Partición: " + nombre + "\n"
           "  Disco: " + disco;
}

#endif