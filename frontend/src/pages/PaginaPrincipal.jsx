import Terminal from '../components/Terminal';

function PaginaPrincipal({ sesionActiva, onLogin, onLogout, onIrAVisualizar, onIrALogin }) {
    return (
        <div style={estilos.pagina}>
            <header style={estilos.header}>
                <h1 style={estilos.titulo}>🖥️ Sistema de Archivos EXT2/EXT3</h1>
                <p style={estilos.subtitulo}>Manejo e Implementación de Archivos — MIA 2026</p>
            </header>

            <div style={estilos.contenido}>
                <div style={estilos.barraAcciones}>
                    {!sesionActiva.activa ? (
                        <button
                            style={estilos.btnSesion}
                            onClick={onIrALogin}>
                            🔐 Iniciar Sesión
                        </button>
                    ) : (
                        <div style={estilos.infoSesion}>
                            <span style={estilos.textoSesion}>
                                ✅ Sesión activa: <strong>{sesionActiva.usuario}</strong>
                                &nbsp;| Partición: <strong>{sesionActiva.id}</strong>
                            </span>
                            <button
                                style={estilos.btnCerrar}
                                onClick={onLogout}>
                                🚪 Cerrar Sesión
                            </button>
                            <button
                                style={estilos.btnVisualizar}
                                onClick={onIrAVisualizar}>
                                📁 Visualizador
                            </button>
                        </div>
                    )}
                </div>

                <Terminal
                    sesionActiva={sesionActiva}
                    onLogin={onLogin}
                    onLogout={onLogout}
                />
            </div>
        </div>
    );
}

const estilos = {
    pagina: {
        minHeight: '100vh',
        background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
        padding: '20px'
    },
    header: {
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white',
        padding: '30px',
        borderRadius: '15px 15px 0 0',
        textAlign: 'center'
    },
    titulo: {
        margin: 0,
        fontSize: '2em'
    },
    subtitulo: {
        margin: '5px 0 0',
        opacity: 0.9
    },
    contenido: {
        background: 'white',
        padding: '30px',
        borderRadius: '0 0 15px 15px',
        boxShadow: '0 10px 40px rgba(0,0,0,0.3)'
    },
    barraAcciones: {
        display: 'flex',
        justifyContent: 'flex-end',
        marginBottom: '20px'
    },
    infoSesion: {
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
    },
    textoSesion: {
        fontSize: '0.95em',
        color: '#333'
    },
    btnSesion: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    btnCerrar: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #e53935, #ef9a9a)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    btnVisualizar: {
        padding: '10px 20px',
        background: 'linear-gradient(135deg, #56ab2f, #a8e063)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    }
};

export default PaginaPrincipal;