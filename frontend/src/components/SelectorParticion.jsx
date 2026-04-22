function SelectorParticion({ disco, particiones, cargando, onSeleccionar, particionActual }) {

    const formatearBytes = (bytes) => {
        if (!bytes || bytes === 0) return '—';
        if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
        if (bytes >= 1024)        return (bytes / 1024).toFixed(2) + ' KB';
        return bytes + ' B';
    };

    const nombreTipo = (tipo) => {
        if (tipo === 'P') return 'Primaria';
        if (tipo === 'E') return 'Extendida';
        if (tipo === 'L') return 'Lógica';
        return tipo || '—';
    };

    if (cargando) {
        return <div style={estilos.cargando}>⏳ Cargando particiones de <strong>{disco?.name}</strong>...</div>;
    }

    if (!particiones || particiones.length === 0) {
        return (
            <div style={estilos.vacio}>
                No hay particiones montadas en <strong>{disco?.name}</strong>.
                <p style={estilos.hint}>Use la terminal para montar particiones en este disco.</p>
            </div>
        );
    }

    return (
        <div>
            <h3 style={estilos.subtitulo}>
                Seleccione la partición de <strong>{disco?.name}</strong>:
            </h3>
            <div style={estilos.grid}>
                {particiones.map((part, i) => {
                    const seleccionada = particionActual?.mountId === part.mountId;
                    return (
                        <button
                            key={i}
                            style={{
                                ...estilos.tarjeta,
                                ...(seleccionada ? estilos.tarjetaSeleccionada : {})
                            }}
                            onClick={() => onSeleccionar(part)}
                        >
                            <span style={estilos.icono}>🗂️</span>
                            <span style={estilos.nombre}>{part.name}</span>

                            <div style={estilos.tabla}>
                                <div style={estilos.fila}>
                                    <span style={estilos.etiqueta}>ID</span>
                                    <span style={estilos.valor}>{part.mountId}</span>
                                </div>
                                <div style={estilos.fila}>
                                    <span style={estilos.etiqueta}>Tamaño</span>
                                    <span style={estilos.valor}>{formatearBytes(part.size)}</span>
                                </div>
                                <div style={estilos.fila}>
                                    <span style={estilos.etiqueta}>Tipo</span>
                                    <span style={estilos.valor}>{nombreTipo(part.type)}</span>
                                </div>
                                <div style={estilos.fila}>
                                    <span style={estilos.etiqueta}>Fit</span>
                                    <span style={estilos.valor}>{part.fit}</span>
                                </div>
                                <div style={estilos.fila}>
                                    <span style={estilos.etiqueta}>Estado</span>
                                    <span style={{
                                        ...estilos.badge,
                                        ...(part.status === 'Montada'
                                            ? estilos.badgeMontada
                                            : estilos.badgeDesmontada)
                                    }}>
                                        {part.status}
                                    </span>
                                </div>
                            </div>
                        </button>
                    );
                })}
            </div>
        </div>
    );
}

const estilos = {
    subtitulo: {
        color: '#444',
        marginBottom: '16px',
        fontSize: '1em'
    },
    grid: {
        display: 'flex',
        gap: '16px',
        flexWrap: 'wrap'
    },
    tarjeta: {
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '10px',
        padding: '20px',
        background: 'white',
        border: '2px solid #ddd',
        borderRadius: '12px',
        cursor: 'pointer',
        transition: 'all 0.2s',
        minWidth: '180px',
        textAlign: 'center'
    },
    tarjetaSeleccionada: {
        border: '2px solid #2a5298',
        background: '#e8eaf6',
        boxShadow: '0 4px 12px rgba(42,82,152,0.2)'
    },
    icono: {
        fontSize: '2.2em'
    },
    nombre: {
        fontWeight: 'bold',
        color: '#2a5298',
        fontSize: '1.05em'
    },
    tabla: {
        display: 'flex',
        flexDirection: 'column',
        gap: '5px',
        width: '100%'
    },
    fila: {
        display: 'flex',
        justifyContent: 'space-between',
        gap: '8px',
        fontSize: '0.8em'
    },
    etiqueta: {
        color: '#888',
        fontWeight: 'bold'
    },
    valor: {
        color: '#333'
    },
    badge: {
        borderRadius: '4px',
        padding: '1px 8px',
        fontWeight: 'bold',
        fontSize: '0.85em'
    },
    badgeMontada: {
        background: '#c8e6c9',
        color: '#2e7d32'
    },
    badgeDesmontada: {
        background: '#ffcdd2',
        color: '#c62828'
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
        color: '#aaa',
        marginTop: '8px'
    }
};

export default SelectorParticion;