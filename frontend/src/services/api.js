import axios from 'axios';

const api = axios.create({
    baseURL: '/api'
});

export const ejecutarScript = async (script) => {
    const response = await api.post('/execute-script', { script });
    return response.data;
};

export const loginGrafico = async (id, usuario, password, diskPath, partitionName) => {
    const response = await api.post('/login', { id, usuario, password, diskPath, partitionName });
    return response.data;
};

export const logoutAPI = async (id, diskPath, partitionName, usuario, password) => {
    const response = await api.post('/logout', { id, diskPath, partitionName, usuario, password });
    return response.data;
};

export const obtenerDiscos = async () => {
    const response = await api.get('/disks');
    return response.data;
};

export const obtenerInfoDisco = async (diskPath, partitions) => {
    const response = await api.post('/disks/partitions', { diskPath, partitions });
    return response.data;
};

export const listarDirectorio = async (params) => {
    const response = await api.post('/filesystem', params);
    return response.data;
};

export const leerArchivo = async (params) => {
    const response = await api.post('/filesystem/file', params);
    return response.data;
};

export const verJournal = async (params) => {
    const response = await api.get('/journaling', {
        params: {
            id:            params.mountId,
            diskPath:      params.diskPath,
            partitionName: params.partitionName,
            usuario:       params.usuario,
            password:      params.password
        }
    });
    return response.data;
};

export const verificarServidor = async () => {
    const response = await api.get('/health');
    return response.data;
};

export default api;