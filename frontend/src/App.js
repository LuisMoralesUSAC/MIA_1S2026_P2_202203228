import { useState } from 'react';
import PaginaPrincipal from './pages/PaginaPrincipal';
import PaginaLogin from './pages/PaginaLogin';
import SelectorDiscos from './components/SelectorDiscos';
import ExplorarArchivos from './components/ExplorarArchivos';

function App() {
    const [pagina, setPagina] = useState('principal');
    const [sesionActiva, setSesionActiva] = useState({
        activa: false,
        usuario: '',
        password: '',
        id: ''
    });
    const [discoSeleccionado, setDiscoSeleccionado] = useState(null);
    const [particionSeleccionada, setParticionSeleccionada] = useState(null);

    const handleLogin = (usuario, id, password = '') => {
        setSesionActiva({ activa: true, usuario, password, id });
        setPagina('principal');
    };

    const handleLogout = async () => {
        setSesionActiva({ activa: false, usuario: '', id: '' });
        setDiscoSeleccionado(null);
        setParticionSeleccionada(null);
    };

    const handleIrALogin = () => {
        setPagina('login');
    };

    const handleIrAVisualizar = () => {
        setPagina('visualizador');
    };

    const handleSeleccionarDisco = (disco) => {
        setDiscoSeleccionado(disco);
        setParticionSeleccionada(null);
    };

    const handleSeleccionarParticion = (particion, disco) => {
        setParticionSeleccionada(particion);
    };

    if (pagina === 'login') {
        return (
            <PaginaLogin
                onLoginExitoso={handleLogin}
                onVolver={() => setPagina('principal')}
            />
        );
    }

    if (pagina === 'visualizador') {
        return (
            <div style={estilos.pagina}>
                <header style={estilos.header}>
                    <h1 style={estilos.titulo}>🖥️ Sistema de Archivos EXT2/EXT3</h1>
                    <p style={estilos.subtitulo}>Manejo e Implementación de Archivos — MIA 2026</p>
                </header>
                <div style={estilos.contenido}>
                    <div style={estilos.barraAcciones}>
                        <button style={estilos.btnVolver} onClick={() => {
                            setPagina('principal');
                            setParticionSeleccionada(null);
                        }}>
                            ← Terminal
                        </button>
                        {sesionActiva.activa && !particionSeleccionada && (
                            <span style={estilos.textoSesion}>
                                ✅ <strong>{sesionActiva.usuario}</strong> | {sesionActiva.id}
                            </span>
                        )}
                        {particionSeleccionada && (
                            <button
                                style={estilos.btnVolver}
                                onClick={() => setParticionSeleccionada(null)}
                            >
                                ← Selección de partición
                            </button>
                        )}
                    </div>
                    
                    {particionSeleccionada ? (
                        <ExplorarArchivos
                            particion={particionSeleccionada}
                            disco={discoSeleccionado}
                            sesion={sesionActiva}
                            onCerrarSesion={handleLogout}
                        />
                    ) : (
                        <SelectorDiscos
                            onSeleccionarDisco={handleSeleccionarDisco}
                            onSeleccionarParticion={handleSeleccionarParticion}
                            discoActual={discoSeleccionado}
                            particionActual={particionSeleccionada}
                        />
                    )}
                </div>
            </div>
        );
    }

    return (
        <PaginaPrincipal
            sesionActiva={sesionActiva}
            onLogin={handleLogin}
            onLogout={handleLogout}
            onIrAVisualizar={handleIrAVisualizar}
            onIrALogin={handleIrALogin}
        />
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
        padding: '20px 30px',
        borderRadius: '15px 15px 0 0',
        textAlign: 'center'
    },
    titulo: { margin: 0, fontSize: '1.8em' },
    subtitulo: { margin: '4px 0 0', opacity: 0.9, fontSize: '0.9em' },
    contenido: {
        background: 'white',
        padding: '24px 30px',
        borderRadius: '0 0 15px 15px',
        boxShadow: '0 10px 40px rgba(0,0,0,0.3)'
    },
    barraAcciones: {
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        marginBottom: '20px',
        paddingBottom: '16px',
        borderBottom: '1px solid #eee'
    },
    textoSesion: { flex: 1, fontSize: '0.9em', color: '#333' },
    btnVolver: {
        padding: '8px 16px',
        background: 'transparent',
        color: '#666',
        border: '1px solid #ddd',
        borderRadius: '8px',
        cursor: 'pointer'
    },
    btnCerrar: {
        padding: '8px 16px',
        background: 'linear-gradient(135deg, #e53935, #ef9a9a)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    }
};

export default App;