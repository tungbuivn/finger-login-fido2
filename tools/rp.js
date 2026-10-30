const http = require('http');
const html = `<!doctype html><meta charset="utf-8"><title>FIDO2 Local RP</title>
<h1>FIDO2 Local RP</h1>
<button id="reg" style="font-size:24px;padding:20px">Register FIDO2</button>
<pre id="out"></pre>
<script>
document.getElementById('reg').onclick = async () => {
  const out = document.getElementById('out');
  out.textContent = 'waiting...';
  try {
    const c = await navigator.credentials.create({publicKey:{
      challenge: crypto.getRandomValues(new Uint8Array(32)),
      rp: {id: 'localhost', name: 'FIDO2 Local'},
      user: {id: new Uint8Array([1,2,3,4,5,6,7,8]), name: 'fido2test', displayName: 'FIDO2 Test'},
      pubKeyCredParams: [{type:'public-key', alg:-7}],
      authenticatorSelection: {authenticatorAttachment:'cross-platform', userVerification:'discouraged', residentKey:'discouraged'},
      timeout: 20000,
      attestation: 'none'
    }});
    out.textContent = 'OK ' + c.id;
  } catch (e) {
    out.textContent = 'ERR ' + e.name + ' ' + e.message;
  }
};
</script>`;
http.createServer((req, res) => {
  res.writeHead(200, {'Content-Type':'text/html; charset=utf-8'});
  res.end(html);
}).listen(8099, '127.0.0.1', () => console.log('RP on http://localhost:8099'));
