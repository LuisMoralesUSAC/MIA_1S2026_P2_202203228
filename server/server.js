const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = 4000;

app.use(cors());
app.use(bodyParser.json({ limit: '10mb' }));
app.use(bodyParser.urlencoded({ extended: true }));

const DISK_MANAGER_PATH = path.join(__dirname, '../backend/disk_manager');
const registeredDisks = new Map();
const mountOrder = []; // [{diskPath, partName, mountId}]
let sesionActivaServidor = null;

function runCommands(lines) {
    return new Promise((resolve, reject) => {
        const child = spawn(DISK_MANAGER_PATH, []);
        let stdout = '';
        let stderr = '';

        child.stdout.on('data', d => { stdout += d.toString(); });
        child.stderr.on('data', d => { stderr += d.toString(); });

        child.on('close', () => resolve(stdout));
        child.on('error', err => reject(err));

        try {
            lines.forEach(cmd => child.stdin.write(cmd + '\n'));
            child.stdin.write('exit\n');
            child.stdin.end();
        } catch (err) {
            reject(err);
        }
    });
}

function expandPath(p) {
    return (p || '').replace(/^~/, process.env.HOME || '');
}

function parsearLs(contenido) {
    const entradas = [];
    for (const linea of contenido.split('\n')) {
        const partes = linea.split('|').map(p => p.trim());
        if (partes.length < 7) continue;
        if (partes[0] === 'Permisos' || /^[-+\s]+$/.test(linea)) continue;
        const nombre = partes[6];
        if (nombre && nombre !== '.' && nombre !== '..') {
            entradas.push({
                permisos: partes[0],
                owner:    partes[1],
                grupo:    partes[2],
                size:     partes[3],
                fecha:    partes[4],
                tipo:     partes[5],
                nombre
            });
        }
    }
    return entradas;
}

function parsearJournal(stdout) {
    const entradas = [];
    for (const linea of stdout.split('\n')) {
        if (!linea.includes('|')) continue;
        if (/^[-+\s]+$/.test(linea)) continue;
        const partes = linea.split('|').map(p => p.trim()).filter(p => p.length > 0);
        if (partes.length < 4) continue;
        if (partes[0] === 'Operacion' || partes[0] === 'Operación') continue;
        const [op, rutaP, cont, fecha] = partes;
        if (op && rutaP) {
            entradas.push({ operacion: op, path: rutaP, contenido: cont || '—', fecha: fecha || '—' });
        }
    }
    return entradas;
}

app.post('/api/execute-script', async (req, res) => {
    const { script, sesion } = req.body;
    if (!script) {
        return res.status(400).json({ error: 'No se proporcionó ningún script' });
    }

    const lines = script
        .split('\n')
        .map(l => l.trim())
        .filter(l => l && !l.startsWith('#'));

    if (lines.length === 0) {
        return res.json({ results: [] });
    }

    // Actualizar sesión del servidor si el frontend envió una
    if (sesion && sesion.activa) {
        sesionActivaServidor = {
            usuario:  sesion.usuario,
            password: sesion.password,
            id:       sesion.id
        };
    }

    const prepCommands = mountOrder.map(m =>
        `mount -path=${m.diskPath} -name=${m.partName}`
    );

    // Si hay sesión activa, agregar login automático
    if (sesionActivaServidor) {
        prepCommands.push(
            `login -user=${sesionActivaServidor.usuario} -pass=${sesionActivaServidor.password} -id=${sesionActivaServidor.id}`
        );
    }
    const allCommands = [...prepCommands, ...lines];

    try {
        const stdout = await runCommands(allCommands);
        const parts = stdout.split(/\n> /).map(p => p.trim());
        const scriptParts = parts.slice(prepCommands.length);
        const results = lines.map((cmd, i) => {
            let output = (scriptParts[i] || '').replace(/\nSaliendo del programa\.\.\.\s*$/, '').trim();
            return { command: cmd, output, error: null };
        });

        // Detectar mounts exitosos en el script
        results.forEach(r => {
            if (r.output && r.output.includes('Partición montada exitosamente')) {
                const matchId   = r.output.match(/ID:\s*(\S+)/);
                const matchDisk = r.output.match(/Disco:\s*(.+)/);
                const matchPart = r.output.match(/Partición:\s*(\S+)/);
                if (matchId && matchDisk && matchPart) {
                    const mountId  = matchId[1].trim();
                    const diskPath = matchDisk[1].trim();
                    const partName = matchPart[1].trim();

                    if (!registeredDisks.has(diskPath)) {
                        registeredDisks.set(diskPath, {
                            path: diskPath,
                            name: path.basename(diskPath),
                            partitions: []
                        });
                    }
                    const disk = registeredDisks.get(diskPath);
                    if (!disk.partitions.find(p => p.mountId === mountId)) {
                        disk.partitions.push({ name: partName, mountId });
                    }
                    if (!mountOrder.find(m => m.mountId === mountId)) {
                        mountOrder.push({ diskPath, partName, mountId });
                    }
                }
            }

            // Detectar unmounts
            if (r.output && r.output.includes('Partición desmontada exitosamente')) {
                const matchId = r.output.match(/ID:\s*(\S+)/);
                if (matchId) {
                    const mountId = matchId[1].trim();
                    for (const [, diskInfo] of registeredDisks.entries()) {
                        diskInfo.partitions = diskInfo.partitions.filter(p => p.mountId !== mountId);
                    }
                    const idx = mountOrder.findIndex(m => m.mountId === mountId);
                    if (idx !== -1) mountOrder.splice(idx, 1);
                }
            }

            // Detectar login desde script
            if (r.output && r.output.includes('Sesión iniciada exitosamente')) {
                const matchUser = r.output.match(/Usuario: (\S+)/);
                const matchId   = r.output.match(/ID Partición: (\S+)/);
                if (matchUser && matchId) {
                }
            }

            // Detectar logout desde script
            if (r.output && r.output.includes('Sesión cerrada exitosamente')) {
                sesionActivaServidor = null;
            }
        });

        res.json({ results });
    } catch (err) {
        res.status(500).json({ error: 'Error ejecutando el binario C++', details: err.message });
    }
});


app.post('/api/login', async (req, res) => {
    const { id, usuario, password, diskPath, partitionName } = req.body;

    if (!id || !usuario || !password) {
        return res.status(400).json({
            success: false,
            message: 'Faltan parámetros: id, usuario y password son obligatorios'
        });
    }

    // Buscar disco y partición desde registeredDisks
    let resolvedDiskPath = diskPath ? expandPath(diskPath) : null;
    let resolvedPartName = partitionName || null;

    if (!resolvedDiskPath || !resolvedPartName) {
        for (const [dPath, diskInfo] of registeredDisks.entries()) {
            const found = diskInfo.partitions.find(p => p.mountId === id);
            if (found) {
                resolvedDiskPath = dPath;
                resolvedPartName = found.name;
                break;
            }
        }
    }

    if (!resolvedDiskPath || !resolvedPartName) {
        return res.status(404).json({
            success: false,
            message: `No se encontró ninguna partición con ID '${id}'. Asegúrese de haber ejecutado mount desde la terminal primero.`
        });
    }

    if (!fs.existsSync(resolvedDiskPath)) {
        return res.status(404).json({
            success: false,
            message: `Disco no encontrado: ${resolvedDiskPath}`
        });
    }

    try {
        const idxObjetivo = mountOrder.findIndex(m => m.mountId === id);
        if (idxObjetivo === -1) {
            return res.status(404).json({
                success: false,
                message: `No se encontró el ID '${id}' en el historial de mounts.`
            });
        }

        const mountsNecesarios = mountOrder.slice(0, idxObjetivo + 1);
        const comandos = [
            ...mountsNecesarios.map(m => `mount -path=${m.diskPath} -name=${m.partName}`),
            `login -user=${usuario} -pass=${password} -id=${id}`
        ];

        const stdout = await runCommands(comandos);
        const success = stdout.includes('Sesión iniciada exitosamente');

        // Guardar sesión activa en el servidor
        if (success) {
            sesionActivaServidor = { usuario, password: password, id };
        }

        res.json({
            success,
            message: success ? 'Login exitoso' : 'Credenciales incorrectas o partición no encontrada'
        });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.post('/api/logout', async (req, res) => {
    const { id, diskPath, partitionName, usuario, password } = req.body;
    if (!id) return res.status(400).json({ success: false, message: 'Falta id' });

    const expandedPath = expandPath(diskPath);

    try {
        await runCommands([
            `mount -path=${expandedPath} -name=${partitionName}`,
            `login -user=${usuario} -pass=${password} -id=${id}`,
            'logout'
        ]);
        res.json({ success: true, message: 'Sesión cerrada' });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.get('/api/disks', (req, res) => {
    const disks = Array.from(registeredDisks.values()).map(d => ({
        path: d.path,
        name: d.name,
        partitions: d.partitions
    }));
    res.json({ success: true, disks });
});

app.post('/api/disks/partitions', async (req, res) => {
    const { diskPath, partitions } = req.body;
    if (!diskPath || !partitions || partitions.length === 0) {
        return res.json({ success: true, partitions: [] });
    }

    const expandedPath = expandPath(diskPath);
    const mountLines = partitions.map(p => `mount -path=${expandedPath} -name=${p.name}`);

    try {
        const stdout = await runCommands([...mountLines, 'mounted']);
        const bloques = stdout.split('---');
        const particionesInfo = partitions.map(p => {
            const bloque = bloques.find(b => b.includes(`Partición: ${p.name}`)) || '';
            const getId   = b => { const m = b.match(/ID:\s*(\S+)/);     return m ? m[1] : p.mountId; };
            const getSize = b => { const m = b.match(/Tamaño:\s*(\d+)/); return m ? parseInt(m[1]) : 0; };
            const getType = b => { const m = b.match(/Tipo:\s*(\S+)/);   return m ? m[1] : '?'; };

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
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.post('/api/filesystem', async (req, res) => {
    const { mountId, dirPath, diskPath, partitionName, usuario, password } = req.body;
    if (!mountId || !dirPath) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros: mountId y dirPath son obligatorios' });
    }

    const expandedPath = expandPath(diskPath);
    const lsOutPath = `/tmp/mia_ls_${Date.now()}.txt`;

    try {
        await runCommands([
            `mount -path=${expandedPath} -name=${partitionName}`,
            `login -user=${usuario} -pass=${password} -id=${mountId}`,
            `rep -name=ls -path=${lsOutPath} -id=${mountId} -path_file_ls=${dirPath}`
        ]);

        let entradas = [];
        if (fs.existsSync(lsOutPath)) {
            const contenido = fs.readFileSync(lsOutPath, 'utf8');
            fs.unlinkSync(lsOutPath);
            entradas = parsearLs(contenido);
        }

        res.json({ success: true, entradas });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.post('/api/filesystem/file', async (req, res) => {
    const { mountId, filePath, diskPath, partitionName, usuario, password } = req.body;
    if (!mountId || !filePath) {
        return res.status(400).json({ success: false, message: 'Faltan parámetros: mountId y filePath son obligatorios' });
    }

    const expandedPath = expandPath(diskPath);

    try {
        const stdout = await runCommands([
            `mount -path=${expandedPath} -name=${partitionName}`,
            `login -user=${usuario} -pass=${password} -id=${mountId}`,
            `cat -file1=${filePath}`
        ]);

        // Extraer solo la salida del cat
        const parts = stdout.split(/\n> /).map(p => p.trim());
        let contenido = '';

        for (const parte of parts) {
            if (
                parte.includes('=== MOUNT ===') ||
                parte.includes('Partición montada') ||
                parte.includes('ya está montada') ||
                parte.includes('Sesión iniciada') ||
                parte.includes('Saliendo del programa') ||
                parte.startsWith('exit') ||
                parte === ''
            ) continue;
            contenido = parte;
            break;
        }

        res.json({ success: true, contenido: contenido || '(archivo vacío)' });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.get('/api/journaling', async (req, res) => {
    const { id, diskPath, partitionName, usuario, password } = req.query;
    if (!id) {
        return res.status(400).json({ success: false, message: 'Falta el parámetro id' });
    }

    const expandedPath = expandPath(diskPath);

    try {
        const stdout = await runCommands([
            `mount -path=${expandedPath} -name=${partitionName}`,
            `login -user=${usuario} -pass=${password} -id=${id}`,
            `journaling -id=${id}`
        ]);

        const entradas = parsearJournal(stdout);
        res.json({ success: true, entradas });
    } catch (err) {
        res.status(500).json({ success: false, message: err.message });
    }
});

app.get('/api/health', (req, res) => {
    res.json({
        status: 'ok',
        diskManagerPath: DISK_MANAGER_PATH,
        diskManagerExists: fs.existsSync(DISK_MANAGER_PATH),
        disksRegistered: registeredDisks.size
    });
});

app.listen(PORT, () => {
    console.log(`Servidor corriendo en http://localhost:${PORT}`);
    console.log(`Binario C++: ${DISK_MANAGER_PATH}`);
    console.log(`Binario existe: ${fs.existsSync(DISK_MANAGER_PATH)}`);
});