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

export default api;