import axios from 'axios';

const api = axios.create({
    baseURL: '/api'
});

export const ejecutarScript = async (script) => {
    const response = await api.post('/execute-script', { script });
    return response.data;
};

export const loginAPI = async (id, usuario, password) => {
    const response = await api.post('/execute-script', {
        script: `login -user=${usuario} -pass=${password} -id=${id}`
    });
    return response.data;
};

export const logoutAPI = async () => {
    const response = await api.post('/execute-script', {
        script: 'logout'
    });
    return response.data;
};

export const loginGrafico = async (id, usuario, password, diskPath, partitionName) => {
    const response = await api.post('/login', { id, usuario, password, diskPath, partitionName });
    return response.data;
};

export const obtenerDiscos = async () => {
    const response = await api.get('/disks');
    return response.data;
};

export const obtenerInfoDisco = async (diskPath, partitions) => {
    const response = await api.post('/disks/info', { diskPath, partitions });
    return response.data;
};

export const listarDirectorio = async (params) => {
    const response = await api.post('/filesystem/ls', params);
    return response.data;
};

export const leerArchivo = async (params) => {
    const response = await api.post('/filesystem/cat', params);
    return response.data;
};

export default api;