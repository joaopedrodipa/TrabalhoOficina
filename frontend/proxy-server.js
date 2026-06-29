// Mini-proxy local — contorna o bloqueio de CORS da API
// e mantém a chave de API fora do código do frontend.
//
// Como usar (PowerShell):
//   node "e:\TrabalhoOficina\frontend\proxy-server.js"


const http = require('http');

const API_BASE = 'https://apisilas.ddns.net/api/v1';
const API_KEY  = 'SUA_CHAVE_API_AQUI';
const PORT     = 3001;

function setCors(res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

const server = http.createServer((req, res) => {
  setCors(res);

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  const url = new URL(req.url, `http://localhost:${PORT}`);

  if (req.method === 'GET' && url.pathname === '/telemetria') {
    const limite = url.searchParams.get('limite') || '1';
    fetch(`${API_BASE}/telemetria?chave_api=${API_KEY}&limite=${limite}`)
      .then(async apiRes => {
        const data = await apiRes.text();
        res.writeHead(apiRes.status, { 'Content-Type': 'application/json' });
        res.end(data);
      })
      .catch(e => {
        res.writeHead(502, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ erro: 'Falha ao buscar dados', detalhe: String(e) }));
      });
    return;
  }

  if (req.method === 'POST' && url.pathname === '/comandos') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      const params  = new URLSearchParams(body);
      const comando = params.get('comando') || '';
      fetch(`${API_BASE}/comandos/enviar_externo?chave_api=${API_KEY}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'comando=' + encodeURIComponent(comando)
      })
        .then(async apiRes => {
          const data = await apiRes.text();
          res.writeHead(apiRes.status, { 'Content-Type': 'application/json' });
          res.end(data);
        })
        .catch(e => {
          res.writeHead(502, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ erro: 'Falha ao enviar comando', detalhe: String(e) }));
        });
    });
    return;
  }

  res.writeHead(404, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ erro: 'Rota não encontrada' }));
});

server.listen(PORT, () => {
  console.log(`Proxy rodando em http://localhost:${PORT}`);
  console.log('Deixe esta janela aberta enquanto usa o index.html.');
});
