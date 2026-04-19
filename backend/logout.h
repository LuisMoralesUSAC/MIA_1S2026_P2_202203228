#ifndef LOGOUT_H
#define LOGOUT_H

#include <string>
#include "session.h"

std::string comandoLogout() {
    if (!SessionManager::currentSession.isLoggedIn()) {
        return "Error: No hay ninguna sesión activa";
    }

    std::string usuario_anterior = SessionManager::currentSession.getCurrentUser();
    std::string id_anterior = SessionManager::currentSession.currentID;
    
    SessionManager::currentSession.logout();
    
    std::string mensaje = "Sesión cerrada exitosamente\n";
    mensaje += "Usuario: " + usuario_anterior + "\n";
    mensaje += "ID Partición: " + id_anterior;
    
    return mensaje;
}

#endif