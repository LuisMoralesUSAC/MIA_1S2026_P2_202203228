import { useState } from 'react';
import { loginGrafico } from '../services/api';

function PaginaLogin({ onLoginExitoso, onVolver }) {
    const [idParticion, setIdParticion] = useState('');
    const [usuario, setUsuario]         = useState('');
    const [password, setPassword]       = useState('');
    const [recordar, setRecordar]       = useState(false);
    const [error, setError]             = useState('');
    const [cargando, setCargando]       = useState(false);

    const manejarLogin = async () => {
        if (!idParticion.trim() || !usuario.trim() || !password.trim()) {
            setError('Todos los campos son obligatorios');
            return;
        }

        setCargando(true);
        setError('');

        try {
            const data = await loginGrafico(idParticion.trim(), usuario.trim(), password.trim(), '', '');

            if (data.success) {
                if (recordar) {
                    localStorage.setItem('mia_usuario', usuario.trim());
                    localStorage.setItem('mia_id', idParticion.trim());
                }
                onLoginExitoso(usuario.trim(), idParticion.trim(), password.trim());
            } else {
                setError(data.message || 'Credenciales incorrectas o partición no montada');
            }
        } catch (err) {
            setError('Error de conexión con el servidor: ' + (err.response?.data?.message || err.message));
        }

        setCargando(false);
    };

    const manejarTecla = (e) => {
        if (e.key === 'Enter') manejarLogin();
    };

    return (
        <div style={estilos.pagina}>
            <div style={estilos.tarjeta}>
                <div style={estilos.encabezado}>
                    <h2 style={estilos.titulo}>🔐 Iniciar Sesión</h2>
                    <p style={estilos.subtitulo}>Sistema de Archivos EXT2/EXT3</p>
                </div>

                <div style={estilos.formulario}>
                    <div style={estilos.campo}>
                        <label style={estilos.etiqueta}>ID Partición</label>
                        <input
                            style={estilos.input}
                            type="text"
                            placeholder="Ej: 281A"
                            value={idParticion}
                            onChange={e => setIdParticion(e.target.value)}
                            onKeyDown={manejarTecla}
                        />
                    </div>

                    <div style={estilos.campo}>
                        <label style={estilos.etiqueta}>Usuario</label>
                        <input
                            style={estilos.input}
                            type="text"
                            placeholder="Ej: root"
                            value={usuario}
                            onChange={e => setUsuario(e.target.value)}
                            onKeyDown={manejarTecla}
                        />
                    </div>

                    <div style={estilos.campo}>
                        <label style={estilos.etiqueta}>Contraseña</label>
                        <input
                            style={estilos.input}
                            type="password"
                            placeholder="••••••••"
                            value={password}
                            onChange={e => setPassword(e.target.value)}
                            onKeyDown={manejarTecla}
                        />
                    </div>

                    <div style={estilos.campoCheck}>
                        <input
                            type="checkbox"
                            id="recordar"
                            checked={recordar}
                            onChange={e => setRecordar(e.target.checked)}
                        />
                        <label htmlFor="recordar" style={estilos.etiquetaCheck}>
                            Recordar usuario
                        </label>
                    </div>

                    {error && (
                        <div style={estilos.error}>
                            ⚠️ {error}
                        </div>
                    )}

                    <button
                        style={{...estilos.btnSubmit, opacity: cargando ? 0.7 : 1}}
                        onClick={manejarLogin}
                        disabled={cargando}>
                        {cargando ? '⏳ Verificando...' : 'Ingresar'}
                    </button>

                    <button
                        style={estilos.btnVolver}
                        onClick={onVolver}>
                        ← Volver a la Terminal
                    </button>
                </div>
            </div>
        </div>
    );
}

const estilos = {
    pagina: {
        minHeight: '100vh',
        background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '20px'
    },
    tarjeta: {
        background: 'white',
        borderRadius: '15px',
        boxShadow: '0 10px 40px rgba(0,0,0,0.3)',
        width: '100%',
        maxWidth: '420px',
        overflow: 'hidden'
    },
    encabezado: {
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white',
        padding: '30px',
        textAlign: 'center'
    },
    titulo: {
        margin: 0,
        fontSize: '1.8em'
    },
    subtitulo: {
        margin: '5px 0 0',
        opacity: 0.9,
        fontSize: '0.9em'
    },
    formulario: {
        padding: '30px',
        display: 'flex',
        flexDirection: 'column',
        gap: '16px'
    },
    campo: {
        display: 'flex',
        flexDirection: 'column',
        gap: '6px'
    },
    etiqueta: {
        fontWeight: 'bold',
        color: '#333',
        fontSize: '0.9em'
    },
    input: {
        padding: '10px 14px',
        border: '2px solid #ddd',
        borderRadius: '8px',
        fontSize: '14px',
        outline: 'none',
        transition: 'border-color 0.2s'
    },
    campoCheck: {
        display: 'flex',
        alignItems: 'center',
        gap: '8px'
    },
    etiquetaCheck: {
        fontSize: '0.9em',
        color: '#555',
        cursor: 'pointer'
    },
    error: {
        background: '#fff3f3',
        border: '1px solid #ffcdd2',
        color: '#c62828',
        padding: '10px 14px',
        borderRadius: '8px',
        fontSize: '0.9em'
    },
    btnSubmit: {
        padding: '12px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '1em'
    },
    btnVolver: {
        padding: '10px',
        background: 'transparent',
        color: '#666',
        border: '1px solid #ddd',
        borderRadius: '8px',
        cursor: 'pointer',
        fontSize: '0.9em'
    }
};

export default PaginaLogin;