import { useState, useEffect, useCallback } from 'react';
import { listarDirectorio, leerArchivo } from '../services/api';
import VisualizadorJournal from './VisualizadorJournal';

function ExplorarArchivos({ particion, disco, sesion, onCerrarSesion }) {
    const [rutaActual, setRutaActual]     = useState('/');
    const [entradas, setEntradas]         = useState([]);
    const [cargando, setCargando]         = useState(false);
    const [error, setError]               = useState('');
    const [archivoAbierto, setArchivoAbierto] = useState(null);
    const [contenidoArchivo, setContenidoArchivo] = useState('');
    const [cargandoArchivo, setCargandoArchivo]   = useState(false);
    const [mostrarJournal, setMostrarJournal] = useState(false);

    const params = useCallback(() => ({
        mountId:       particion.mountId,
        diskPath:      disco.path,
        partitionName: particion.name,
        usuario:       sesion.usuario,
        password:      sesion.password
    }), [particion, disco, sesion]);

    const cargarDirectorio = useCallback(async (ruta) => {
        setCargando(true);
        setError('');
        setArchivoAbierto(null);
        try {
            const data = await listarDirectorio({ ...params(), dirPath: ruta });
            if (data.success) {
                setEntradas(data.entradas || []);
                setRutaActual(ruta);
            } else {
                setError(data.message || 'Error al leer directorio');
            }
        } catch (e) {
            setError('Error de conexión: ' + e.message);
        }
        setCargando(false);
    }, [params]);

    useEffect(() => {
        cargarDirectorio('/');
    }, [cargarDirectorio]);

    const abrirCarpeta = (nombre) => {
        const nueva = rutaActual === '/'
            ? '/' + nombre
            : rutaActual + '/' + nombre;
        cargarDirectorio(nueva);
    };

    const subirNivel = () => {
        if (rutaActual === '/') return;
        const partes = rutaActual.split('/').filter(Boolean);
        partes.pop();
        cargarDirectorio(partes.length === 0 ? '/' : '/' + partes.join('/'));
    };

    const abrirArchivo = async (nombre) => {
        const rutaArchivo = rutaActual === '/'
            ? '/' + nombre
            : rutaActual + '/' + nombre;
        setCargandoArchivo(true);
        setArchivoAbierto(nombre);
        setContenidoArchivo('');
        try {
            const data = await leerArchivo({ ...params(), filePath: rutaArchivo });
            setContenidoArchivo(data.contenido || '(archivo vacío)');
        } catch (e) {
            setContenidoArchivo('Error al leer el archivo');
        }
        setCargandoArchivo(false);
    };

    const cerrarArchivo = () => {
        setArchivoAbierto(null);
        setContenidoArchivo('');
    };

    const breadcrumb = () => {
        const partes = rutaActual.split('/').filter(Boolean);
        const items = [{ label: '/', ruta: '/' }];
        partes.forEach((p, i) => {
            items.push({
                label: p,
                ruta: '/' + partes.slice(0, i + 1).join('/')
            });
        });
        return items;
    };

    const formatearBytes = (s) => {
        const n = parseInt(s);
        if (isNaN(n) || n === 0) return '—';
        if (n >= 1024 * 1024) return (n / (1024 * 1024)).toFixed(1) + ' MB';
        if (n >= 1024)        return (n / 1024).toFixed(1) + ' KB';
        return n + ' B';
    };

    return (
        <div style={estilos.contenedor}>

            {/* Cabecera */}
            <div style={estilos.cabecera}>
                <div style={estilos.infoParticion}>
                    <span style={estilos.nombrePart}>🗂️ {particion.name}</span>
                    <span style={estilos.idPart}>ID: {particion.mountId}</span>
                </div>
                <div style={estilos.accionesCabecera}>
                    <span style={estilos.sesionInfo}>
                        👤 {sesion.usuario}
                    </span>
                    <button style={estilos.btnJournal} onClick={() => setMostrarJournal(true)}>
                        📋 Journal
                    </button>
                    <button style={estilos.btnCerrar} onClick={onCerrarSesion}>
                        🚪 Cerrar Sesión
                    </button>
                </div>
            </div>

            {/* Breadcrumb */}
            <div style={estilos.breadcrumb}>
                {breadcrumb().map((item, i, arr) => (
                    <span key={i} style={estilos.breadcrumbItem}>
                        <button
                            style={{
                                ...estilos.breadcrumbBtn,
                                ...(i === arr.length - 1 ? estilos.breadcrumbActual : {})
                            }}
                            onClick={() => cargarDirectorio(item.ruta)}
                            disabled={i === arr.length - 1}
                        >
                            {item.label}
                        </button>
                        {i < arr.length - 1 && (
                            <span style={estilos.separador}>/</span>
                        )}
                    </span>
                ))}
            </div>

            {/* Barra de navegación */}
            <div style={estilos.barraNav}>
                <button
                    style={{
                        ...estilos.btnNav,
                        opacity: rutaActual === '/' ? 0.4 : 1,
                        cursor: rutaActual === '/' ? 'default' : 'pointer'
                    }}
                    onClick={subirNivel}
                    disabled={rutaActual === '/'}
                >
                    ⬆ Atrás
                </button>
                <button
                    style={estilos.btnNav}
                    onClick={() => cargarDirectorio(rutaActual)}
                >
                    🔄 Recargar
                </button>
                <span style={estilos.rutaTexto}>{rutaActual}</span>
            </div>

            {/* Contenido */}
            {error && <div style={estilos.error}>⚠️ {error}</div>}

            {cargando ? (
                <div style={estilos.cargando}>⏳ Cargando directorio...</div>
            ) : (
                <div style={estilos.grid}>
                    {entradas.length === 0 && !error && (
                        <div style={estilos.vacio}>Directorio vacío</div>
                    )}
                    {entradas.map((entrada, i) => {
                        const esCarpeta = entrada.tipo === 'Carpeta';
                        return (
                            <button
                                key={i}
                                style={{
                                    ...estilos.tarjeta,
                                    ...(archivoAbierto === entrada.nombre
                                        ? estilos.tarjetaActiva : {})
                                }}
                                onClick={() => esCarpeta
                                    ? abrirCarpeta(entrada.nombre)
                                    : abrirArchivo(entrada.nombre)
                                }
                                title={`Permisos: ${entrada.permisos} | Owner: ${entrada.owner}`}
                            >
                                <span style={estilos.icono}>
                                    {esCarpeta ? '📁' : '📄'}
                                </span>
                                <span style={estilos.nombreEntrada}>{entrada.nombre}</span>
                                <div style={estilos.metaEntrada}>
                                    <span>{formatearBytes(entrada.size)}</span>
                                    <span style={{
                                        ...estilos.badgeTipo,
                                        ...(esCarpeta
                                            ? estilos.badgeCarpeta
                                            : estilos.badgeArchivo)
                                    }}>
                                        {entrada.tipo}
                                    </span>
                                </div>
                                <span style={estilos.permisos}>{entrada.permisos}</span>
                            </button>
                        );
                    })}
                </div>
            )}

            {/* Modal de archivo */}
            {archivoAbierto && (
                <div style={estilos.modalOverlay} onClick={cerrarArchivo}>
                    <div style={estilos.modal} onClick={e => e.stopPropagation()}>
                        <div style={estilos.modalCabecera}>
                            <span style={estilos.modalTitulo}>
                                📄 {archivoAbierto}
                            </span>
                            <button style={estilos.btnModalCerrar} onClick={cerrarArchivo}>
                                ✕
                            </button>
                        </div>
                        <div style={estilos.modalRuta}>
                            {rutaActual === '/'
                                ? '/' + archivoAbierto
                                : rutaActual + '/' + archivoAbierto}
                        </div>
                        <div style={estilos.modalContenido}>
                            {cargandoArchivo
                                ? '⏳ Cargando contenido...'
                                : contenidoArchivo}
                        </div>
                    </div>
                </div>
            )}

            {mostrarJournal && (
                <VisualizadorJournal
                    particion={particion}
                    disco={disco}
                    sesion={sesion}
                    onCerrar={() => setMostrarJournal(false)}
                />
            )}
        </div>
    );
}

const estilos = {
    contenedor: {
        display: 'flex',
        flexDirection: 'column',
        gap: '12px'
    },
    cabecera: {
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '12px 16px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        borderRadius: '10px',
        color: 'white'
    },
    infoParticion: {
        display: 'flex',
        flexDirection: 'column',
        gap: '2px'
    },
    nombrePart: {
        fontWeight: 'bold',
        fontSize: '1.1em'
    },
    idPart: {
        fontSize: '0.8em',
        opacity: 0.8
    },
    accionesCabecera: {
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
    },
    sesionInfo: {
        fontSize: '0.9em',
        opacity: 0.9
    },
    btnCerrar: {
        padding: '6px 14px',
        background: '#e53935',
        color: 'white',
        border: 'none',
        borderRadius: '6px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '0.85em'
    },
    breadcrumb: {
        display: 'flex',
        alignItems: 'center',
        flexWrap: 'wrap',
        gap: '2px',
        padding: '8px 12px',
        background: '#f5f5f5',
        borderRadius: '8px',
        fontSize: '0.9em'
    },
    breadcrumbItem: {
        display: 'flex',
        alignItems: 'center',
        gap: '2px'
    },
    breadcrumbBtn: {
        background: 'none',
        border: 'none',
        color: '#2a5298',
        cursor: 'pointer',
        fontWeight: 'bold',
        padding: '2px 4px',
        borderRadius: '4px',
        fontSize: '0.95em'
    },
    breadcrumbActual: {
        color: '#333',
        cursor: 'default'
    },
    separador: {
        color: '#aaa',
        margin: '0 2px'
    },
    barraNav: {
        display: 'flex',
        alignItems: 'center',
        gap: '10px'
    },
    btnNav: {
        padding: '7px 14px',
        background: 'linear-gradient(135deg, #2196F3, #00BCD4)',
        color: 'white',
        border: 'none',
        borderRadius: '7px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '0.85em'
    },
    rutaTexto: {
        fontFamily: 'monospace',
        color: '#555',
        fontSize: '0.9em'
    },
    grid: {
        display: 'flex',
        flexWrap: 'wrap',
        gap: '12px',
        minHeight: '100px'
    },
    tarjeta: {
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '6px',
        padding: '16px 14px',
        background: 'white',
        border: '2px solid #eee',
        borderRadius: '10px',
        cursor: 'pointer',
        transition: 'all 0.15s',
        minWidth: '110px',
        maxWidth: '130px'
    },
    tarjetaActiva: {
        border: '2px solid #2a5298',
        background: '#e8eaf6'
    },
    icono: {
        fontSize: '2.2em'
    },
    nombreEntrada: {
        fontWeight: 'bold',
        fontSize: '0.82em',
        color: '#333',
        wordBreak: 'break-all',
        textAlign: 'center'
    },
    metaEntrada: {
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '4px',
        fontSize: '0.75em',
        color: '#666'
    },
    badgeTipo: {
        borderRadius: '4px',
        padding: '1px 6px',
        fontWeight: 'bold',
        fontSize: '0.85em'
    },
    badgeCarpeta: {
        background: '#fff9c4',
        color: '#f57f17'
    },
    badgeArchivo: {
        background: '#e3f2fd',
        color: '#1565c0'
    },
    permisos: {
        fontSize: '0.7em',
        color: '#aaa',
        fontFamily: 'monospace'
    },
    error: {
        background: '#fff3f3',
        border: '1px solid #ffcdd2',
        color: '#c62828',
        padding: '10px 14px',
        borderRadius: '8px'
    },
    cargando: {
        color: '#666',
        fontStyle: 'italic',
        padding: '30px',
        textAlign: 'center'
    },
    vacio: {
        color: '#aaa',
        padding: '40px',
        textAlign: 'center',
        width: '100%'
    },
    modalOverlay: {
        position: 'fixed',
        top: 0, left: 0, right: 0, bottom: 0,
        background: 'rgba(0,0,0,0.5)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000
    },
    modal: {
        background: 'white',
        borderRadius: '12px',
        width: '90%',
        maxWidth: '650px',
        maxHeight: '80vh',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: '0 20px 60px rgba(0,0,0,0.3)'
    },
    modalCabecera: {
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '16px 20px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white'
    },
    modalTitulo: {
        fontWeight: 'bold',
        fontSize: '1em'
    },
    btnModalCerrar: {
        background: 'none',
        border: 'none',
        color: 'white',
        fontSize: '1.2em',
        cursor: 'pointer'
    },
    modalRuta: {
        padding: '8px 20px',
        background: '#f5f5f5',
        fontFamily: 'monospace',
        fontSize: '0.85em',
        color: '#555'
    },
    modalContenido: {
        padding: '20px',
        fontFamily: 'monospace',
        fontSize: '0.9em',
        overflowY: 'auto',
        whiteSpace: 'pre-wrap',
        wordBreak: 'break-word',
        flex: 1,
        color: '#222'
    },
    btnJournal: {
        padding: '6px 14px',
        background: 'linear-gradient(135deg, #FF9800, #FFC107)',
        color: 'white',
        border: 'none',
        borderRadius: '6px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '0.85em'
    }
};

export default ExplorarArchivos;