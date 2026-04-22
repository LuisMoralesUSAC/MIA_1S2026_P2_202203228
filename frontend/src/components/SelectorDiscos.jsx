import { useState, useEffect } from 'react';
import { obtenerDiscos, obtenerInfoDisco } from '../services/api';
import SelectorParticion from './SelectorParticion';

function SelectorDiscos({ onSeleccionarDisco, onSeleccionarParticion, discoActual, particionActual }) {
    const [discos, setDiscos] = useState([]);
    const [discoSeleccionado, setDiscoSeleccionado] = useState(null);
    const [particiones, setParticiones] = useState([]);
    const [cargando, setCargando] = useState(false);
    const [cargandoParticiones, setCargandoParticiones] = useState(false);
    const [error, setError] = useState('');

    useEffect(() => {
        cargarDiscos();
    }, []);

    const cargarDiscos = async () => {
        setCargando(true);
        setError('');
        try {
            const data = await obtenerDiscos();
            if (data.success) {
                setDiscos(data.disks);
            }
        } catch (e) {
            setError('No se pudo conectar con el servidor');
        }
        setCargando(false);
    };

    const seleccionarDisco = async (disco) => {
        setDiscoSeleccionado(disco);
        setParticiones([]);
        onSeleccionarDisco(disco);

        if (!disco.partitions || disco.partitions.length === 0) return;

        setCargandoParticiones(true);
        try {
            const data = await obtenerInfoDisco(disco.path, disco.partitions);
            if (data.success) {
                setParticiones(data.partitions);
            }
        } catch (e) {
            setParticiones(disco.partitions.map(p => ({
                name: p.name,
                mountId: p.mountId,
                size: 0,
                type: '?',
                status: 'Montada'
            })));
        }
        setCargandoParticiones(false);
    };

    return (
        <div style={estilos.contenedor}>
            <div style={estilos.encabezado}>
                <h2 style={estilos.titulo}>📀 Visualizador del Sistema de Archivos</h2>
                <button style={estilos.btnRefrescar} onClick={cargarDiscos}>
                    🔄 Refrescar
                </button>
            </div>

            {error && <div style={estilos.error}>{error}</div>}

            {cargando ? (
                <div style={estilos.cargando}>Cargando discos...</div>
            ) : (
                <>
                    <section style={estilos.seccion}>
                        <h3 style={estilos.subtitulo}>Seleccione el disco que desea visualizar:</h3>
                        {discos.length === 0 ? (
                            <div style={estilos.vacio}>
                                <p>No hay discos registrados.</p>
                                <p style={estilos.hint}>
                                    Use la terminal para crear y montar particiones,
                                    luego inicie sesión para que el disco aparezca aquí.
                                </p>
                            </div>
                        ) : (
                            <div style={estilos.gridDiscos}>
                                {discos.map((disco, i) => (
                                    <button
                                        key={i}
                                        style={{
                                            ...estilos.tarjetaDisco,
                                            ...(discoActual?.path === disco.path
                                                ? estilos.tarjetaSeleccionada : {})
                                        }}
                                        onClick={() => seleccionarDisco(disco)}
                                    >
                                        <span style={estilos.iconoDisco}>💽</span>
                                        <span style={estilos.nombreDisco}>{disco.name}</span>
                                        <span style={estilos.infoDisco}>
                                            {disco.partitions?.length || 0} partición(es) montada(s)
                                        </span>
                                    </button>
                                ))}
                            </div>
                        )}
                    </section>

                    {discoSeleccionado && (
                        <section style={estilos.seccion}>
                            <SelectorParticion
                                disco={discoSeleccionado}
                                particiones={particiones}
                                cargando={cargandoParticiones}
                                onSeleccionar={onSeleccionarParticion}
                                particionActual={particionActual}
                            />
                        </section>
                    )}
                </>
            )}
        </div>
    );
}

const estilos = {
    contenedor: {
        padding: '20px'
    },
    encabezado: {
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '24px'
    },
    titulo: {
        margin: 0,
        color: '#1e3c72',
        fontSize: '1.4em'
    },
    btnRefrescar: {
        padding: '8px 16px',
        background: 'linear-gradient(135deg, #2196F3, #00BCD4)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold'
    },
    seccion: {
        marginBottom: '32px'
    },
    subtitulo: {
        color: '#444',
        marginBottom: '16px',
        fontSize: '1em'
    },
    gridDiscos: {
        display: 'flex',
        gap: '16px',
        flexWrap: 'wrap'
    },
    tarjetaDisco: {
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '8px',
        padding: '20px 24px',
        background: 'white',
        border: '2px solid #ddd',
        borderRadius: '12px',
        cursor: 'pointer',
        transition: 'all 0.2s',
        minWidth: '120px'
    },
    tarjetaSeleccionada: {
        border: '2px solid #1e3c72',
        background: '#e8eaf6',
        boxShadow: '0 4px 12px rgba(30,60,114,0.2)'
    },
    iconoDisco: {
        fontSize: '2.5em'
    },
    nombreDisco: {
        fontWeight: 'bold',
        color: '#1e3c72',
        fontSize: '0.9em'
    },
    infoDisco: {
        fontSize: '0.75em',
        color: '#666'
    },
    gridParticiones: {
        display: 'flex',
        gap: '16px',
        flexWrap: 'wrap'
    },
    tarjetaParticion: {
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '8px',
        padding: '20px 24px',
        background: 'white',
        border: '2px solid #ddd',
        borderRadius: '12px',
        cursor: 'pointer',
        transition: 'all 0.2s',
        minWidth: '160px'
    },
    iconoParticion: {
        fontSize: '2em'
    },
    nombrePart: {
        fontWeight: 'bold',
        color: '#2a5298',
        fontSize: '1em'
    },
    detallesPart: {
        display: 'flex',
        flexDirection: 'column',
        gap: '4px',
        fontSize: '0.78em',
        color: '#555',
        textAlign: 'center'
    },
    badgeMontada: {
        background: '#c8e6c9',
        color: '#2e7d32',
        borderRadius: '4px',
        padding: '2px 8px',
        fontWeight: 'bold',
        marginTop: '4px'
    },
    cargando: {
        color: '#666',
        fontStyle: 'italic',
        padding: '20px'
    },
    vacio: {
        color: '#888',
        padding: '20px',
        textAlign: 'center',
        background: '#f5f5f5',
        borderRadius: '8px'
    },
    hint: {
        fontSize: '0.85em',
        color: '#aaa'
    },
    error: {
        background: '#fff3f3',
        border: '1px solid #ffcdd2',
        color: '#c62828',
        padding: '10px 14px',
        borderRadius: '8px',
        marginBottom: '16px'
    }
};

export default SelectorDiscos;