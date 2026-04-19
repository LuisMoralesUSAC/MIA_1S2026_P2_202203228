#ifndef SESSION_H
#define SESSION_H

#include <string>
#include <iostream>

namespace SessionManager {
    
    struct Session {
        std::string currentUser;
        std::string currentID;
        int uid;
        int gid;
        bool isRoot;
        bool active;
        
        Session() {
            currentUser = "";
            currentID = "";
            uid = -1;
            gid = -1;
            isRoot = false;
            active = false;
        }
        
        bool isLoggedIn() const {
            return active;
        }
        
        std::string getCurrentUser() const {
            return currentUser;
        }
        
        void login(const std::string& user, const std::string& id, 
                   int userUid, int userGid) {
            currentUser = user;
            currentID = id;
            uid = userUid;
            gid = userGid;
            isRoot = (uid == 1 && gid == 1);
            active = true;
        }
        
        void logout() {
            currentUser = "";
            currentID = "";
            uid = -1;
            gid = -1;
            isRoot = false;
            active = false;
        }
        
        std::string getInfo() const {
            if (!active) {
                return "No hay sesión activa";
            }
            
            std::string info = "Sesión activa:\n";
            info += "  Usuario: " + currentUser + "\n";
            info += "  ID Partición: " + currentID + "\n";
            info += "  UID: " + std::to_string(uid) + "\n";
            info += "  GID: " + std::to_string(gid) + "\n";
            info += "  Es root: " + std::string(isRoot ? "Sí" : "No");
            return info;
        }
    };
    
    static Session currentSession;
    
    inline bool isSessionActive() {
        return currentSession.isLoggedIn();
    }
    
    inline bool isCurrentUserRoot() {
        return currentSession.isRoot;
    }
    
}

#endif