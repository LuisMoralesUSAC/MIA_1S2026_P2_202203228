const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const { spawn } = require('child_process');
const path = require('path');

const app = express();
const PORT = 3000;

app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

const DISK_MANAGER_PATH = path.join(__dirname, '../backend/disk_manager');

app.post('/api/execute-script', (req, res) => {
    const { script } = req.body;
    
    if (!script) {
        return res.status(400).json({ error: 'No se proporcionó ningún script' });
    }
    
    const lines = script.split('\n').filter(line => {
        const trimmed = line.trim();
        return trimmed && !trimmed.startsWith('#');
    });
    
    const child = spawn(DISK_MANAGER_PATH, []);
    
    let stdout = '';
    let stderr = '';
    let results = [];
    
    child.stdout.on('data', (data) => {
        stdout += data.toString();
    });
    
    child.stderr.on('data', (data) => {
        stderr += data.toString();
    });
    
    child.on('close', (code) => {
        const outputs = stdout.split('\n\n').filter(o => o.trim());
        
        lines.forEach((command, index) => {
            results.push({
                command: command,
                output: outputs[index] || '',
                error: null
            });
        });
        
        if (stderr) {
            results.push({
                command: 'ERROR',
                output: '',
                error: stderr
            });
        }
        
        res.json({ results });
    });

    child.on('error', (error) => {
        res.status(500).json({ 
            error: 'Error ejecutando comandos',
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