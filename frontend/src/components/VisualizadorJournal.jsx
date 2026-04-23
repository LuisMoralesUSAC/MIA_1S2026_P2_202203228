import { useState } from 'react';
import { verJournal } from '../services/api';

function VisualizadorJournal({ particion, disco, sesion, onCerrar }) {
    const [entradas, setEntradas]     = useState([]);
    const [cargando, setCargando]     = useState(false);
    const [error, setError]           = useState('');
    const [cargado, setCargado]       = useState(false);
    const [filtroOp, setFiltroOp]     = useState('');
    const [paginaActual, setPagina]   = useState(1);
    const FILAS_POR_PAGINA = 10;

    const cargar = async () => {
        setCargando(true);
        setError('');
        try {
            const data = await verJournal({
                mountId:       particion.mountId,
                diskPath:      disco.path,
                partitionName: particion.name,
                usuario:       sesion.usuario,
                password:      sesion.password
            });
            if (data.success) {
                setEntradas(data.entradas || []);
                setCargado(true);
                setPagina(1);
            } else {
                setError(data.message || 'Error al leer el journal');
            }
        } catch (e) {
            setError('Error de conexión: ' + e.message);
        }
        setCargando(false);
    };

    const operacionesUnicas = [...new Set(entradas.map(e => e.operacion))];

    const entradasFiltradas = filtroOp
        ? entradas.filter(e => e.operacion === filtroOp)
        : entradas;

    const totalPaginas = Math.ceil(entradasFiltradas.length / FILAS_POR_PAGINA);
    const inicio = (paginaActual - 1) * FILAS_POR_PAGINA;
    const entradasPagina = entradasFiltradas.slice(inicio, inicio + FILAS_POR_PAGINA);

    return (
        <div style={estilos.overlay} onClick={onCerrar}>
            <div style={estilos.modal} onClick={e => e.stopPropagation()}>

                {/* Cabecera */}
                <div style={estilos.cabecera}>
                    <div>
                        <span style={estilos.titulo}>📋 Journal — {particion.name}</span>
                        <span style={estilos.subtitulo}> ID: {particion.mountId}</span>
                    </div>
                    <button style={estilos.btnCerrar} onClick={onCerrar}>✕</button>
                </div>

                {/* Barra de herramientas */}
                <div style={estilos.toolbar}>
                    <button
                        style={{ ...estilos.btnCargar, opacity: cargando ? 0.6 : 1 }}
                        onClick={cargar}
                        disabled={cargando}
                    >
                        {cargando ? '⏳ Cargando...' : (cargado ? '🔄 Recargar' : '📂 Cargar Journal')}
                    </button>

                    {cargado && operacionesUnicas.length > 0 && (
                        <select
                            style={estilos.select}
                            value={filtroOp}
                            onChange={e => { setFiltroOp(e.target.value); setPagina(1); }}
                        >
                            <option value="">Todas las operaciones</option>
                            {operacionesUnicas.map((op, i) => (
                                <option key={i} value={op}>{op}</option>
                            ))}
                        </select>
                    )}

                    {cargado && (
                        <span style={estilos.contador}>
                            {entradasFiltradas.length} entrada(s)
                        </span>
                    )}
                </div>

                {/* Error */}
                {error && <div style={estilos.error}>⚠️ {error}</div>}

                {/* Tabla */}
                {cargado && (
                    <>
                        {entradasFiltradas.length === 0 ? (
                            <div style={estilos.vacio}>
                                {filtroOp ? 'No hay entradas para esta operación.' : 'Sin transacciones registradas.'}
                            </div>
                        ) : (
                            <div style={estilos.tablaWrapper}>
                                <table style={estilos.tabla}>
                                    <thead>
                                        <tr>
                                            <th style={estilos.th}>#</th>
                                            <th style={estilos.th}>Operación</th>
                                            <th style={estilos.th}>Path</th>
                                            <th style={estilos.th}>Contenido</th>
                                            <th style={estilos.th}>Fecha</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {entradasPagina.map((e, i) => (
                                            <tr key={i} style={i % 2 === 0 ? estilos.trPar : estilos.trImpar}>
                                                <td style={estilos.tdNum}>{inicio + i + 1}</td>
                                                <td style={estilos.tdOp}>
                                                    <span style={{
                                                        ...estilos.badge,
                                                        ...obtenerColorOp(e.operacion)
                                                    }}>
                                                        {e.operacion}
                                                    </span>
                                                </td>
                                                <td style={estilos.tdPath}>{e.path}</td>
                                                <td style={estilos.tdContent}>{e.contenido || '—'}</td>
                                                <td style={estilos.tdFecha}>{e.fecha}</td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}

                        {/* Paginación */}
                        {totalPaginas > 1 && (
                            <div style={estilos.paginacion}>
                                <button
                                    style={estilos.btnPag}
                                    onClick={() => setPagina(p => Math.max(1, p - 1))}
                                    disabled={paginaActual === 1}
                                >
                                    ‹ Anterior
                                </button>
                                <span style={estilos.paginaInfo}>
                                    Página {paginaActual} de {totalPaginas}
                                </span>
                                <button
                                    style={estilos.btnPag}
                                    onClick={() => setPagina(p => Math.min(totalPaginas, p + 1))}
                                    disabled={paginaActual === totalPaginas}
                                >
                                    Siguiente ›
                                </button>
                            </div>
                        )}
                    </>
                )}

                {!cargado && !cargando && !error && (
                    <div style={estilos.vacio}>
                        Presione "Cargar Journal" para ver las transacciones de esta partición.
                    </div>
                )}
            </div>
        </div>
    );
}

function obtenerColorOp(op) {
    const mapa = {
        mkdir:   { background: '#e3f2fd', color: '#1565c0' },
        mkfile:  { background: '#e8f5e9', color: '#2e7d32' },
        remove:  { background: '#ffebee', color: '#c62828' },
        rename:  { background: '#fff3e0', color: '#e65100' },
        copy:    { background: '#f3e5f5', color: '#6a1b9a' },
        move:    { background: '#fce4ec', color: '#880e4f' },
        chown:   { background: '#e0f2f1', color: '#00695c' },
        chmod:   { background: '#f9fbe7', color: '#558b2f' },
        mkusr:   { background: '#e8eaf6', color: '#283593' },
        rmusr:   { background: '#fbe9e7', color: '#bf360c' },
        mkgrp:   { background: '#e0f7fa', color: '#006064' },
    };
    const key = (op || '').toLowerCase().trim();
    return mapa[key] || { background: '#f5f5f5', color: '#333' };
}

const estilos = {
    overlay: {
        position: 'fixed',
        top: 0, left: 0, right: 0, bottom: 0,
        background: 'rgba(0,0,0,0.55)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 2000
    },
    modal: {
        background: 'white',
        borderRadius: '14px',
        width: '95%',
        maxWidth: '900px',
        maxHeight: '85vh',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        boxShadow: '0 24px 64px rgba(0,0,0,0.35)'
    },
    cabecera: {
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '16px 22px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white'
    },
    titulo: {
        fontWeight: 'bold',
        fontSize: '1.1em'
    },
    subtitulo: {
        fontSize: '0.85em',
        opacity: 0.8
    },
    btnCerrar: {
        background: 'none',
        border: 'none',
        color: 'white',
        fontSize: '1.3em',
        cursor: 'pointer'
    },
    toolbar: {
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        padding: '14px 22px',
        borderBottom: '1px solid #eee',
        flexWrap: 'wrap'
    },
    btnCargar: {
        padding: '8px 18px',
        background: 'linear-gradient(135deg, #1e3c72, #2a5298)',
        color: 'white',
        border: 'none',
        borderRadius: '8px',
        cursor: 'pointer',
        fontWeight: 'bold',
        fontSize: '0.9em'
    },
    select: {
        padding: '7px 12px',
        border: '1px solid #ddd',
        borderRadius: '7px',
        fontSize: '0.88em',
        cursor: 'pointer'
    },
    contador: {
        marginLeft: 'auto',
        fontSize: '0.85em',
        color: '#666'
    },
    error: {
        margin: '12px 22px',
        padding: '10px 14px',
        background: '#fff3f3',
        border: '1px solid #ffcdd2',
        color: '#c62828',
        borderRadius: '8px',
        fontSize: '0.9em'
    },
    tablaWrapper: {
        overflowY: 'auto',
        flex: 1,
        padding: '0 22px'
    },
    tabla: {
        width: '100%',
        borderCollapse: 'collapse',
        fontSize: '0.88em',
        marginTop: '12px'
    },
    th: {
        padding: '10px 12px',
        background: '#f5f5f5',
        borderBottom: '2px solid #ddd',
        textAlign: 'left',
        fontWeight: 'bold',
        color: '#444',
        position: 'sticky',
        top: 0
    },
    trPar:   { background: 'white' },
    trImpar: { background: '#fafafa' },
    tdNum: {
        padding: '9px 12px',
        color: '#aaa',
        fontSize: '0.85em',
        textAlign: 'center',
        borderBottom: '1px solid #f0f0f0',
        width: '40px'
    },
    tdOp: {
        padding: '9px 12px',
        borderBottom: '1px solid #f0f0f0',
        width: '110px'
    },
    tdPath: {
        padding: '9px 12px',
        fontFamily: 'monospace',
        fontSize: '0.85em',
        borderBottom: '1px solid #f0f0f0',
        color: '#333',
        wordBreak: 'break-all'
    },
    tdContent: {
        padding: '9px 12px',
        fontFamily: 'monospace',
        fontSize: '0.82em',
        color: '#555',
        borderBottom: '1px solid #f0f0f0',
        maxWidth: '200px',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap'
    },
    tdFecha: {
        padding: '9px 12px',
        fontSize: '0.82em',
        color: '#666',
        borderBottom: '1px solid #f0f0f0',
        whiteSpace: 'nowrap'
    },
    badge: {
        display: 'inline-block',
        padding: '2px 8px',
        borderRadius: '5px',
        fontWeight: 'bold',
        fontSize: '0.85em'
    },
    paginacion: {
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        gap: '16px',
        padding: '14px 22px',
        borderTop: '1px solid #eee'
    },
    btnPag: {
        padding: '6px 14px',
        background: 'linear-gradient(135deg, #2196F3, #00BCD4)',
        color: 'white',
        border: 'none',
        borderRadius: '7px',
        cursor: 'pointer',
        fontSize: '0.85em',
        fontWeight: 'bold'
    },
    paginaInfo: {
        fontSize: '0.88em',
        color: '#555'
    },
    vacio: {
        padding: '40px',
        textAlign: 'center',
        color: '#aaa',
        fontStyle: 'italic'
    }
};

export default VisualizadorJournal;