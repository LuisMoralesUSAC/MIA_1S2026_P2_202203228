import { useState } from 'react';
import PaginaPrincipal from './pages/PaginaPrincipal';
import PaginaLogin from './pages/PaginaLogin';

function App() {
    const [pagina, setPagina] = useState('principal');
    const [sesionActiva, setSesionActiva] = useState({
        activa: false,
        usuario: '',
        id: ''
    });

    const handleLogin = (usuario, id) => {
        setSesionActiva({ activa: true, usuario, id });
        setPagina('principal');
    };

    const handleLogout = async () => {
        setSesionActiva({ activa: false, usuario: '', id: '' });
    };

    const handleIrALogin = () => {
        setPagina('login');
    };

    const handleIrAVisualizar = () => {
        alert('Visualizador próximamente - Fase 7');
    };

    if (pagina === 'login') {
        return (
            <PaginaLogin
                onLoginExitoso={handleLogin}
                onVolver={() => setPagina('principal')}
            />
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

export default App;