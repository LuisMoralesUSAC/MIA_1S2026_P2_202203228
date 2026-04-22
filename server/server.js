const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = 4000;

app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

const DISK_MANAGER_PATH = path.join(__dirname, '../backend/disk_manager');
const registeredDisks = new Map();

app.post('/api/execute-script', (req, res) => {
    const { script } = req.body;

    if (!script) {
        return res.status(400).json({ error: 'No se proporcionó ningún script' });
    }

    const lines = script.split('\n').map(l => l.trim()).filter(line => {
        return line && !line.startsWith('#');
    });

    if (lines.length === 0) {
        return res.json({ results: [] });
    }

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (data) => { stdout += data.toString(); });
    child.stderr.on('data', (data) => { stderr += data.toString(); });

    child.on('close', () => {
        const rawLines = stdout.split('\n');
        const results = [];
        let currentOutput = [];
        let cmdIndex = 0;

        for (const rawLine of rawLines) {
            if (rawLine.startsWith('> ')) {
                // Guardar salida del comando anterior
                if (cmdIndex > 0 && cmdIndex - 1 < lines.length) {
                    results[cmdIndex - 1] = {
                        command: lines[cmdIndex - 1],
                        output: currentOutput.join('\n').trim(),
                        error: null
                    };
                }
                currentOutput = [];
                cmdIndex++;
            } else {
                currentOutput.push(rawLine);
            }
        }

        // Guardar último comando
        if (cmdIndex > 0 && cmdIndex - 1 < lines.length) {
            results[cmdIndex - 1] = {
                command: lines[cmdIndex - 1],
                output: currentOutput.join('\n').trim(),
                error: null
            };
        }

        // Rellenar comandos sin resultado
        lines.forEach((cmd, i) => {
            if (!results[i]) {
                results[i] = { command: cmd, output: '', error: null };
            }
        });

        if (stderr) {
            results.push({ command: 'STDERR', output: '', error: stderr });
        }

        res.json({ results });
    });

    child.on('error', (error) => {
        res.status(500).json({
            error: 'Error ejecutando el binario C++',
            details: error.message
        });
    });

    try {
        lines.forEach(command => {
            child.stdin.write(command + '\n');
        });
        child.stdin.write('exit\n');
        child.stdin.end();
    } catch (error) {
        res.status(500).json({
            error: 'Error escribiendo comandos',
            details: error.message
        });
    }
});

app.post('/api/login', (req, res) => {
    const { id, usuario, password, diskPath, partitionName } = req.body;

    if (!id || !usuario || !password) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros' });
    }

    if (!diskPath || !partitionName) {
        return res.status(400).json({ 
            success: false, 
            message: 'Para el login gráfico debe proporcionar diskPath y partitionName' 
        });
    }

    // Construir script completo con mount + login
    const expandedPath = diskPath.replace('~', process.env.HOME || '');
    const script = [
        `mount -path=${expandedPath} -name=${partitionName}`,
        `login -user=${usuario} -pass=${password} -id=${id}`
    ].join('\n');

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (data) => { stdout += data.toString(); });
    child.stderr.on('data', (data) => { stderr += data.toString(); });

    child.on('close', () => {
        const success = stdout.includes('Sesión iniciada exitosamente');

        // Registrar disco si login fue exitoso
        if (success) {
            const expandedP = diskPath.replace('~', process.env.HOME || '');
            if (!registeredDisks.has(expandedP)) {
                registeredDisks.set(expandedP, { path: expandedP, partitions: [] });
            }
            const disk = registeredDisks.get(expandedP);
            if (!disk.partitions.find(p => p.name === partitionName)) {
                disk.partitions.push({ name: partitionName, mountId: id });
            }
        }

        res.json({
            success,
            output: stdout,
            message: success ? 'Login exitoso' : 'Credenciales incorrectas o partición no encontrada'
        });
    });

    child.on('error', (error) => {
        res.status(500).json({ success: false, message: error.message });
    });

    child.stdin.write(script + '\nexit\n');
    child.stdin.end();
});

app.post('/api/disks/register', (req, res) => {
    const { path: diskPath, partitionName, mountId } = req.body;
    if (!diskPath) return res.status(400).json({ success: false });

    if (!registeredDisks.has(diskPath)) {
        registeredDisks.set(diskPath, { path: diskPath, partitions: [] });
    }
    const disk = registeredDisks.get(diskPath);
    if (partitionName && mountId && !disk.partitions.find(p => p.name === partitionName)) {
        disk.partitions.push({ name: partitionName, mountId });
    }
    res.json({ success: true });
});

// Endpoint para listar discos conocidos
app.get('/api/disks', (req, res) => {
    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';

    child.stdout.on('data', d => { stdout += d.toString(); });
    child.on('close', () => {
        const disks = Array.from(registeredDisks.values()).map(d => ({
            path: d.path,
            name: path.basename(d.path),
            partitions: d.partitions
        }));
        res.json({ success: true, disks });
    });
    child.on('error', () => res.json({ success: true, disks: [] }));
    child.stdin.write('exit\n');
    child.stdin.end();
});

// Endpoint para obtener particiones de un disco vía mounted
app.post('/api/disks/info', (req, res) => {
    const { diskPath, partitions } = req.body;
    if (!diskPath || !partitions || partitions.length === 0) {
        return res.json({ success: true, partitions: [] });
    }

    const expandedPath = diskPath.replace('~', process.env.HOME || '');
    const mountLines = partitions.map(p =>
        `mount -path=${expandedPath} -name=${p.name}`
    );
    const script = [...mountLines, 'mounted', 'exit'].join('\n');

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    child.stdout.on('data', d => { stdout += d.toString(); });
    child.on('close', () => {
        // Parsear bloques separados por ---
        const bloques = stdout.split('---');
        const particionesInfo = partitions.map(p => {
            // Buscar el bloque que contenga el nombre de esta partición
            const bloque = bloques.find(b => b.includes(`Partición: ${p.name}`)) || '';

            const getId    = (b) => { const m = b.match(/ID:\s*(\S+)/);      return m ? m[1] : (p.mountId || '?'); };
            const getSize  = (b) => { const m = b.match(/Tamaño:\s*(\d+)/);  return m ? parseInt(m[1]) : 0; };
            const getType  = (b) => { const m = b.match(/Tipo:\s*(\S+)/);    return m ? m[1] : '?'; };

            return {
                name:    p.name,
                mountId: getId(bloque),
                size:    getSize(bloque),
                type:    getType(bloque),
                fit:     p.fit || 'WF',
                status:  bloque.includes(`Partición: ${p.name}`) ? 'Montada' : 'Desmontada'
            };
        });

        res.json({ success: true, partitions: particionesInfo });
    });
    child.on('error', () => res.json({ success: true, partitions: [] }));
    child.stdin.write(script + '\n');
    child.stdin.end();
});

// Obtener contenido de un directorio
app.post('/api/filesystem/directory', (req, res) => {
    const { mountId, path: rutaDir, diskPath, partitionName, usuario, password } = req.body;
    if (!mountId || !rutaDir) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros' });
    }

    const expandedPath = (diskPath || '').replace('~', process.env.HOME || '');
    const script = [
        `mount -path=${expandedPath} -name=${partitionName}`,
        `login -user=${usuario} -pass=${password} -id=${mountId}`,
        `rep -name=ls -path=/tmp/ls_out.jpg -id=${mountId} -path_file_ls=${rutaDir}`,
        'exit'
    ].join('\n');

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    child.stdout.on('data', d => { stdout += d.toString(); });
    child.on('close', () => {
        res.json({ success: true, raw: stdout });
    });
    child.on('error', e => res.status(500).json({ success: false, message: e.message }));
    child.stdin.write(script + '\n');
    child.stdin.end();
});

// Leer contenido de un directorio usando rep ls en texto
app.post('/api/filesystem/ls', (req, res) => {
    const { mountId, dirPath, diskPath, partitionName, usuario, password } = req.body;
    if (!mountId || !dirPath) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros' });
    }

    const expandedPath = (diskPath || '').replace('~', process.env.HOME || '');

    const script = [
        `mount -path=${expandedPath} -name=${partitionName}`,
        `login -user=${usuario} -pass=${password} -id=${mountId}`,
        `ls_json -path=${dirPath}`,
        'exit'
    ].join('\n');

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    child.stdout.on('data', d => { stdout += d.toString(); });

    child.on('close', () => {
        // Extraer la línea JSON de la salida
        const lineas = stdout.split('\n');
        let jsonStr = '';

        for (const linea of lineas) {
            const trimmed = linea.trim();
            if (trimmed.startsWith('[')) {
                jsonStr = trimmed;
                break;
            }
        }

        if (!jsonStr) {
            return res.json({ success: true, entradas: [] });
        }

        try {
            const entradas = JSON.parse(jsonStr);
            res.json({ success: true, entradas });
        } catch (e) {
            res.json({ success: true, entradas: [], parseError: jsonStr });
        }
    });

    child.on('error', e => res.status(500).json({ success: false, message: e.message }));
    child.stdin.write(script + '\n');
    child.stdin.end();
});

function parsearLs(contenido, dirPath) {
    const entradas = [];
    const lineas = contenido.split('\n');
    for (const linea of lineas) {
        const partes = linea.split('|').map(p => p.trim()).filter(Boolean);
        if (partes.length >= 7 && partes[0] !== 'Permisos') {
            entradas.push({
                permisos: partes[0],
                owner:    partes[1],
                grupo:    partes[2],
                size:     partes[3],
                fecha:    partes[4],
                tipo:     partes[5],
                nombre:   partes[6]
            });
        }
    }
    return entradas;
}

// Leer contenido de un archivo
app.post('/api/filesystem/cat', (req, res) => {
    const { mountId, filePath, diskPath, partitionName, usuario, password } = req.body;
    if (!mountId || !filePath) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros' });
    }

    const expandedPath = (diskPath || '').replace('~', process.env.HOME || '');
    const script = [
        `mount -path=${expandedPath} -name=${partitionName}`,
        `login -user=${usuario} -pass=${password} -id=${mountId}`,
        `cat -file1=${filePath}`,
        'exit'
    ].join('\n');

    const child = spawn(DISK_MANAGER_PATH, []);
    let stdout = '';
    child.stdout.on('data', d => { stdout += d.toString(); });
    child.on('close', () => {
        // Extraer solo la salida del cat
        const lineas = stdout.split('\n');
        let capturando = false;
        let contenido = [];
        for (const l of lineas) {
            if (l.includes('Sesión iniciada exitosamente')) { capturando = false; continue; }
            if (l.includes('ID Partición:')) { capturando = true; continue; }
            if (capturando && !l.startsWith('>')) contenido.push(l);
        }
        res.json({ success: true, contenido: contenido.join('\n').trim() });
    });
    child.on('error', e => res.status(500).json({ success: false, message: e.message }));
    child.stdin.write(script + '\n');
    child.stdin.end();
});

app.get('/api/test', (req, res) => {
    res.json({ 
        status: 'ok',
        message: 'Servidor funcionando correctamente',
        diskManagerPath: DISK_MANAGER_PATH
    });
});

app.use(express.static(path.join(__dirname, '../frontend')));

app.listen(PORT, () => {
    console.log(`Servidor corriendo en http://localhost:${PORT}`);
});