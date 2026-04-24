import { useState, useRef, useEffect } from 'react';
import { ejecutarScript } from '../services/api';

function Terminal({ sesionActiva, onLogin, onLogout }) {
    const [input, setInput]   = useState('');
    const [output, setOutput] = useState('Bienvenido al Sistema de Archivos EXT2/EXT3\n');
    const [cargando, setCargando] = useState(false);
    const outputRef = useRef(null);
    const fileInputRef = useRef(null);

    useEffect(() => {
        if (outputRef.current) {
            outputRef.current.scrollTop = outputRef.current.scrollHeight;
        }
    }, [output]);

    const agregarSalida = (texto) => {
        setOutput(prev => prev + texto + '\n');
    };

    const ejecutar = async () => {
        if (!input.trim() || cargando) return;

        setCargando(true);
        agregarSalida('\n> Ejecutando...');

        try {
            const data = await ejecutarScript(input, sesionActiva);

            if (data.results) {
                data.results.forEach(result => {
                    agregarSalida(`> ${result.command}`);
                    if (result.output) agregarSalida(result.output);
                    if (result.error)  agregarSalida(`Error: ${result.error}`);
                });

                const textos = data.results.map(r => r.output || '').join('\n');
                if (textos.includes('Sesión iniciada exitosamente')) {
                    const matchUser = textos.match(/Usuario: (\S+)/);
                    const matchID   = textos.match(/ID Partición: (\S+)/);
                    if (matchUser && matchID) {
                        onLogin(matchUser[1], matchID[1]);
                    }
                }
                if (textos.includes('Sesión cerrada exitosamente')) {
                    onLogout();
                }
            }
        } catch (error) {
            agregarSalida(`Error de conexión: ${error.message}`);
            agregarSalida('Verifique que el servidor esté corriendo en puerto 4000');
        }

        setCargando(false);
    };

    const cargarArchivo = (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (ev) => {
            setInput(ev.target.result);
            agregarSalida(`✓ Archivo cargado: ${file.name}`);
        };
        reader.readAsText(file);
        e.target.value = '';
    };

    const manejarTecla = (e) => {
        if (e.ctrlKey && e.key === 'Enter') ejecutar();
    };

    return (
        <div style={estilos.contenedor}>
            <div style={estilos.seccion}>
                <label style={estilos.etiqueta}>Entrada:</label>
                <textarea
                    style={estilos.textarea}
                    value={input}
                    onChange={e => setInput(e.target.value)}
                    onKeyDown={manejarTecla}
                    placeholder="Ingrese comandos aquí... (Ctrl+Enter para ejecutar)"
                    spellCheck={false}
                />
                <div style={estilos.botones}>
                    <button
                        style={estilos.btnCargar}
                        onClick={() => fileInputRef.current.click()}>
                        📁 Cargar Script
                    </button>
                    <button
                        style={{...estilos.btnEjecutar, opacity: cargando ? 0.6 : 1}}
                        onClick={ejecutar}
                        disabled={cargando}>
                        {cargando ? '⏳ Ejecutando...' : '▶ Ejecutar'}
                    </button>
                    <button
                        style={estilos.btnLimpiar}
                        onClick={() => setOutput('')}>
                        🗑 Limpiar
                    </button>
                </div>
                <input
                    ref={fileInputRef}
                    type="file"
                    accept=".smia,.txt"
                    style={{display: 'none'}}
                    onChange={cargarArchivo}
                />
            </div>

            <div style={estilos.seccion}>
                <label style={estilos.etiqueta}>Salida:</label>
                <div ref={outputRef} style={estilos.output}>
                    {output}
                </div>
            </div>
        </div>
    );
}

const estilos = {
    contenedor: {
        display: 'flex',
        flexDirection: 'column',
        gap: '16px'
    },
    seccion: {
        display: 'flex',
        flexDirection: 'column',
        gap: '8px'
    },
    etiqueta: {
        color: '#1e3c72',
        fontWeight: 'bold',
        fontSize: '1em'
    },
    textarea: {
        width: '100%',
        height: '200px',
        padding: '12px',
        fontFamily: "'Courier New', monospace",
        fontSize: '13px',
        border: '2px solid #ddd',
        borderRadius: '8px',
        resize: 'vertical',
        boxSizing: 'border-box'
    },
    botones: {
        display: 'flex',
        gap: '10px'
    },
    btnCargar: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #2196F3, #00BCD4)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    btnEjecutar: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #56ab2f, #a8e063)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    btnLimpiar: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #e53935, #ef9a9a)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    output: {
        background: '#1e1e1e',
        color: '#00ff00',
        padding: '16px',
        borderRadius: '8px',
        fontFamily: "'Courier New', monospace",
        fontSize: '13px',
        height: '350px',
        overflowY: 'auto',
        whiteSpace: 'pre-wrap',
        wordBreak: 'break-word'
    }
};

export default Terminal;