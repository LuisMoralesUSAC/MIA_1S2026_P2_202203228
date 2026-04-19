#include <iostream>  // Maneja entrada y salida estándar
#include <string>    // Manipula cadenas de texto
#include <sstream>   // Convierte entre tipos de datos y cadenas
#include <cstdlib>   // Proporciona funciones generales como rand() y conversiones de cadenas a números
#include <ctime>     // Trabaja con fechas y horas
#include <algorithm> // Proporciona algoritmos para manipular colecciones de datos.
#include <cctype>    // Funciones para manipular caracteres
#include "structures.h" // Define estructuras de datos personalizadas
#include "mkdisk.h" 
#include "rmdisk.h"    
#include "fdisk.h"     
#include "mount.h"
#include "mkfs.h"
#include "rep.h"
#include "session.h"
#include "login.h"
#include "logout.h"
#include "mkgrp.h"
#include "rmgrp.h"
#include "mkusr.h"
#include "rmusr.h"
#include "chgrp.h"
#include "mkdir.h"
#include "mkfile.h"
#include "cat.h"
#include "journaling.h"
#include "unmount.h"
#include "remove.h"

// Función para convertir string a minúsculas
std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Función para remover comillas de una cadena
std::string removeQuotes(const std::string& str) {
    if (str.length() >= 2 && 
        ((str.front() == '"' && str.back() == '"') || 
         (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.length() - 2);
    }
    return str;
}

// Función para parsear parámetros con soporte para comillas y cualquier orden
std::string parseParameter(const std::string& commandLine, const std::string& paramName) {
    // Buscar el parámetro
    std::string lowerCommandLine = toLowerCase(commandLine);
    std::string lowerParamName = toLowerCase(paramName);
    
    size_t pos = lowerCommandLine.find(lowerParamName + "=");
    if (pos == std::string::npos) {
        return "";
    }
    
    // Encontrar el inicio del valor
    size_t valueStart = pos + paramName.length() + 1;
    if (valueStart >= commandLine.length()) {
        return "";
    }
    
    // Determinar el final del valor
    size_t valueEnd = commandLine.length();
    
    if (commandLine[valueStart] == '"' || commandLine[valueStart] == '\'') {
        char quote = commandLine[valueStart];
        size_t quoteEnd = commandLine.find(quote, valueStart + 1);
        if (quoteEnd != std::string::npos) {
            return commandLine.substr(valueStart + 1, quoteEnd - valueStart - 1);
        } else {
 
            size_t spacePos = commandLine.find(' ', valueStart + 1);
            if (spacePos != std::string::npos) {
                valueEnd = spacePos;
            }
            return commandLine.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
    } else {
        // Si no hay comillas, buscar el siguiente espacio o final de línea
        size_t spacePos = commandLine.find(' ', valueStart);
        if (spacePos != std::string::npos) {
            valueEnd = spacePos;
        }
        return commandLine.substr(valueStart, valueEnd - valueStart);
    }
}

// Función para verificar si existe un flag (parámetro sin valor)
bool hasFlag(const std::string& commandLine, const std::string& flagName) {
    size_t pos = commandLine.find(flagName);
    
    while (pos != std::string::npos) {
        bool inicio_valido = (pos == 0 || commandLine[pos - 1] == ' ');
        size_t end = pos + flagName.length();
        bool fin_valido = (end >= commandLine.length() || 
                          commandLine[end] == ' ' || 
                          commandLine[end] == '\n' ||
                          commandLine[end] == '\r');
        
        if (inicio_valido && fin_valido) {
            return true;
        }
        pos = commandLine.find(flagName, pos + 1);
    }
    
    return false;
}

// Función para parsear y ejecutar comandos
std::string executeCommand(const std::string& commandLine) {
    if (commandLine.empty() || commandLine[0] == '#') {
        return "";
    }
    
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;
    
    // Convertir comando a minúsculas para comparación case-insensitive
    cmd = toLowerCase(cmd);

    if (cmd == "mkdisk") {
        // Parsear parámetros
        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string unit = parseParameter(commandLine, "-unit");
        std::string path = parseParameter(commandLine, "-path");
        
        // Validar parámetros obligatorios
        if (sizeStr.empty() || path.empty()) {
            return "Error: mkdisk requiere parámetros -size y -path\n"
                   "Uso: mkdisk -size=N -unit=[k|m] -path=ruta\n"
                   "Los parámetros pueden estar en cualquier orden";
        }
        
        // Convertir size a entero
        int size;
        try {
            size = std::stoi(sizeStr);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }
        
        if (size <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }
        
        // Unit por defecto es megabytes si no se especifica
        if (unit.empty()) {
            unit = "m";
        } else {
            unit = toLowerCase(unit);
        }
        
        // Validar unidad
        if (unit != "k" && unit != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        return CommandMkdisk::execute(size, unit, path);

    } else if (cmd == "rmdisk") {
        std::string path = parseParameter(commandLine, "-path");

        if (path.empty()) {
            return "Error: rmdisk requiere parámetro -path\n"
                   "Uso: rmdisk -path=ruta";
        }

        return CommandRmdisk::execute(path);

    } else if (cmd == "fdisk") {
        std::string path = parseParameter(commandLine, "-path");
        std::string name = parseParameter(commandLine, "-name");
        std::string deleteName = parseParameter(commandLine, "-delete");
        std::string addStr = parseParameter(commandLine, "-add");
        
        // Validar parámetros obligatorios
        if (path.empty()) {
            return "Error: fdisk requiere parámetro -path\n"
                   "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre\n"
                   "      fdisk -delete=nombre -path=ruta";
        }

        // Si es operación de eliminación
        if (!deleteName.empty()) {
            return CommandFdisk::execute(0, "k", path, "", "", deleteName, "");
        }

        // Si es operación de add
        if (!addStr.empty()) {
            std::string unit = parseParameter(commandLine, "-unit");
            if (unit.empty()) unit = "k";
            else unit = toLowerCase(unit);
            return CommandFdisk::execute(0, unit, path, "", "", "", name, addStr);
        }

        // Si es operación de adición
        if (name.empty()) {
            return "Error: fdisk requiere parámetro -name o -delete\n"
                   "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre\n"
                   "      fdisk -delete=nombre -path=ruta";
        }

        std::string sizeStr = parseParameter(commandLine, "-size");
        if (sizeStr.empty()) {
            return "Error: fdisk requiere parámetro -size para crear particiones\n"
                   "Uso: fdisk -size=N -unit=[k|m] -path=ruta -type=[P|E|L] -fit=[BF|FF|WF] -name=nombre";
        }

        int size;
        try {
            size = std::stoi(sizeStr);
        } catch (const std::exception& e) {
            return "Error: el valor de size debe ser un número entero positivo";
        }

        if (size <= 0) {
            return "Error: el tamaño debe ser un número positivo";
        }

        std::string unit = parseParameter(commandLine, "-unit");
        if (unit.empty()) {
            unit = "k";  // Default: kilobytes
        } else {
            unit = toLowerCase(unit);
        }

        if (unit != "b" && unit != "k" && unit != "m") {
            return "Error: unit debe ser 'k' (kilobytes) o 'm' (megabytes)";
        }

        std::string type = parseParameter(commandLine, "-type");
        if (type.empty()) {
            type = "P";  // Default: primaria
        } else {
            type = toLowerCase(type);
        }

        std::string fit = parseParameter(commandLine, "-fit");
        if (fit.empty()) {
            fit = "WF";  // Default: Worst Fit
        } else {
            fit = toLowerCase(fit);
        }

        return CommandFdisk::execute(size, unit, path, type, fit, "", name);

    } else if (cmd == "mount") {
        std::string path = parseParameter(commandLine, "-path");
        std::string name = parseParameter(commandLine, "-name");
        
        if (path.empty() || name.empty()) {
            return "Error: mount requiere parámetros -path y -name\n"
                   "Uso: mount -path=ruta -name=nombre";
        }
        
        return CommandMount::execute(path, name);

    } else if (cmd == "mkfs") {
        std::string id   = parseParameter(commandLine, "-id");
        std::string type = parseParameter(commandLine, "-type");
        std::string fs   = parseParameter(commandLine, "-fs");
        if (id.empty()) return "Error: mkfs requiere -id";
        std::string result = CommandMkfs::execute(id, type, fs);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result)) CommandJournaling::clearFor(id);
        
        return result;

    } else if (cmd == "rep") {
        std::string name = parseParameter(commandLine, "-name");
        std::string path = parseParameter(commandLine, "-path");
        std::string id = parseParameter(commandLine, "-id");
        std::string pathFileLs = parseParameter(commandLine, "-path_file_ls");
        
        if (name.empty()) {
            return "Error: rep requiere el parámetro -name\n"
                   "Uso: rep -name=<tipo> -path=<ruta> -id=<id> [-path_file_ls=<ruta>]";
        }
        if (path.empty()) {
            return "Error: rep requiere el parámetro -path";
        }
        if (id.empty()) {
            return "Error: rep requiere el parámetro -id";
        }
        
        return CommandRep::execute(name, path, id, pathFileLs);

    } else if (cmd == "mounted") {
        // Mostrar todas las particiones montadas
        return CommandMount::listMountedPartitions();

    } else if (cmd == "unmount") {
        std::string id = parseParameter(commandLine, "-id");
        if (id.empty()) {
            return "Error: unmount requiere el parámetro -id";
        }
        return comandoUnmount(id);
        
    } else if (cmd == "mkusr") {
        std::string user = parseParameter(commandLine, "-user");
        std::string pass = parseParameter(commandLine, "-pass");
        std::string grp  = parseParameter(commandLine, "-grp");
        std::string result = comandoMkusr(user, pass, grp);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            CommandJournaling::add(SessionManager::currentSession.currentID, "mkusr", "/users.txt", user + "," + grp);
        }
        
        return result;

    } else if (cmd == "rmusr") {
        std::string user = parseParameter(commandLine, "-user");
        std::string result = comandoRmusr(user);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            CommandJournaling::add(SessionManager::currentSession.currentID, "rmusr", "/users.txt", user);
        }
        
        return result;

    } else if (cmd == "chgrp") {
        std::string user = parseParameter(commandLine, "-user");
        std::string grp  = parseParameter(commandLine, "-grp");
        std::string result = comandoChgrp(user, grp);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            CommandJournaling::add(SessionManager::currentSession.currentID, "chgrp", "/users.txt", user + "->" + grp);
        }
        
        return result;

    } else if (cmd == "mkdir") {
        std::string dirPath = parseParameter(commandLine, "-path");
        bool hasP = (commandLine.find(" -p") != std::string::npos ||
                     commandLine.find("\t-p") != std::string::npos);
        if (dirPath.empty()) return "Error: mkdir requiere -path";
        std::string result = comandoMkdir(dirPath, hasP);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            CommandJournaling::add(SessionManager::currentSession.currentID, "mkdir", dirPath, hasP ? "-p" : "-");
        }
        
        return result;

    } else if (cmd == "mkfile") {
        std::string filePath = parseParameter(commandLine, "-path");
        bool hasR = (commandLine.find(" -r") != std::string::npos ||
                     commandLine.find("\t-r") != std::string::npos);
        std::string sizeStr = parseParameter(commandLine, "-size");
        std::string cont    = parseParameter(commandLine, "-cont");
        int size = 0;
        if (!sizeStr.empty()) {
            try { size = std::stoi(sizeStr); } catch(...) { size = 0; }
        }
        if (filePath.empty()) return "Error: mkfile requiere -path";
        std::string result = comandoMkfile(filePath, size, cont, hasR);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            std::string detail = !cont.empty() ? cont : ("size=" + std::to_string(size));
            CommandJournaling::add(SessionManager::currentSession.currentID, "mkfile", filePath, detail);
        }
        
        return result;

    } else if (cmd == "cat") {
        std::vector<std::string> archivos;
        
        for (int i = 1; i <= 10; i++) {
            std::string param_name = "-file" + std::to_string(i);
            std::string archivo = parseParameter(commandLine, param_name);
            
            if (!archivo.empty()) {
                archivos.push_back(archivo);
            }
        }
        
        std::string archivo_simple = parseParameter(commandLine, "-file");
        if (!archivo_simple.empty() && archivos.empty()) {
            archivos.push_back(archivo_simple);
        }
        
        if (archivos.empty()) {
            return "Error: Debe especificar al menos un archivo con -file1, -file2, etc.";
        }
        
        return comandoCat(archivos);

    } else if (cmd == "remove") {
        std::string removePath = parseParameter(commandLine, "-path");
        if (removePath.empty()) {
            return "Error: remove requiere el parámetro -path";
        }
        return comandoRemove(removePath);
    
    } else if (cmd == "session") {
        return SessionManager::currentSession.getInfo();
    } else if (cmd == "login") {
    std::string usuario = parseParameter(commandLine, "-user");
    std::string password = parseParameter(commandLine, "-pass");
    std::string id = parseParameter(commandLine, "-id");
    if (usuario.empty() || password.empty() || id.empty()) {
        return "Error: Los parámetros -user, -pass e -id son obligatorios";
    }
    return comandoLogin(usuario, password, id);
    } else if (cmd == "logout") {
    return comandoLogout();

    } else if (cmd == "mkgrp") {
        std::string name = parseParameter(commandLine, "-name");
        std::string result = comandoMkgrp(name);
        
        auto isErrorResult = [](const std::string& r) {
            return r.rfind("Error", 0) == 0 || r.rfind("error", 0) == 0;
        };
        if (!isErrorResult(result) && SessionManager::currentSession.isLoggedIn()) {
            CommandJournaling::add(SessionManager::currentSession.currentID, "mkgrp", "/users.txt", name);
        }
        
        return result;

    } else if (cmd == "rmgrp") {
        std::string nombre = parseParameter(commandLine, "-name");
        if (nombre.empty()) {
            return "Error: El parámetro -name es obligatorio";
        }
    return comandoRmgrp(nombre);

    } else if (cmd == "journaling") {
        std::string id = parseParameter(commandLine, "-id");
        if (id.empty()) return "Error: journaling requiere -id";
        return CommandJournaling::execute(id);

    } else if (cmd == "exit" || cmd == "quit") {
        return "EXIT";
    } else if (cmd.empty()) {
        return "";
    } else {
        return "Error: Comando no reconocido";
    }
}

// Función para ejecutar un comando único y retornar el resultado
std::string executeCommandDirect(const std::string& commandLine) {
    std::string result = executeCommand(commandLine);
    return result;
}

int main(int argc, char* argv[]) {
    // Inicializar semilla para números aleatorios
    srand(time(nullptr));

    // Si hay argumentos, ejecutar en modo no interactivo
    if (argc > 1) {
        std::string command = "";
        for (int i = 1; i < argc; i++) {
            command += argv[i];
            if (i < argc - 1) command += " ";
        }
        
        std::string result = executeCommandDirect(command);
        std::cout << result << std::endl;
        return 0;
    }
    /*
    std::cout << "C++ DISK\n";
    std::cout << "MIA Proyecto 1 - 2026\n";
    std::cout << "Escriba 'exit' para salir\n";
    */
    std::string commandLine;
    while (true) {
        std::cout << "> ";
        std::cout.flush(); 
        
        if (!std::getline(std::cin, commandLine)) {
            //std::cout << "\nSaliendo del programa...\n";
            break;
        }
        
        if (commandLine.empty()) {
            continue;
        }

        // Ejecutar comando
        std::string result = executeCommand(commandLine);

        if (result == "EXIT") {
            std::cout << "Saliendo del programa...\n";
            break;
        }

        // Mostrar resultado
        if (!result.empty()) {
            std::cout << result << "\n\n";
        }
    }

    return 0;
}
