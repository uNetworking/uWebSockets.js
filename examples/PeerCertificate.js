const https = require('https');
const forge = require('node-forge');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
// Generate CA certificate
const ca = generateCACertificate();
const caCertPem = pemEncodeCert(ca.cert);

// Generate server certificate signed by CA
const serverCert = generateCertificate(ca, 'localhost');
const serverCertPem = pemEncodeCert(serverCert.cert);
const serverKeyPem = pemEncodeKey(serverCert.privateKey);

// Generate client certificate signed by CA
const clientCert = generateCertificate(ca, 'client');
const clientCertPem = pemEncodeCert(clientCert.cert);
const clientKeyPem = pemEncodeKey(clientCert.privateKey);

fs.writeFileSync(path.join(__dirname, "server.ca"), caCertPem);
fs.writeFileSync(path.join(__dirname, "server.key"), serverKeyPem);
fs.writeFileSync(path.join(__dirname, "server.cert"), serverCertPem);

const uWS = require('../dist/uws');
const port = 8086;

const app = uWS.SSLApp({
  cert_file_name: path.join(__dirname, "server.cert"),
  key_file_name: path.join(__dirname, "server.key"),
  ca_file_name: path.join(__dirname, "server.ca")
}).get('/*', (res, req) => {
  const certInfo = res.getPeerCertificate();
  console.log(certInfo)
  const certDataBuffer = Buffer.from(certInfo.raw).toString('base64');
  const binaryDerCertificate = forge.util.decode64(certDataBuffer);
  const cert = forge.pki.certificateFromAsn1(forge.asn1.fromDer(binaryDerCertificate));
  const pemCertificate = forge.pki.certificateToPem(cert);
  if (new crypto.X509Certificate(pemCertificate).verify(crypto.createPublicKey(caCertPem))) {
    res.end(`Hello World! your certificate is valid!`);
  }
  else
    res.end('Hello World!');

}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
    sendClientRequest();
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

function generateCACertificate() {
  const keys = forge.pki.rsa.generateKeyPair(2048);
  const cert = forge.pki.createCertificate();
  cert.publicKey = keys.publicKey;
  cert.serialNumber = '01';
  cert.validity.notBefore = new Date();
  cert.validity.notAfter = new Date();
  cert.validity.notAfter.setFullYear(cert.validity.notBefore.getFullYear() + 1);
  const attrs = [
    { name: 'commonName', value: 'example.org' },
    { name: 'countryName', value: 'US' },
    { shortName: 'ST', value: 'California' },
    { name: 'localityName', value: 'San Francisco' },
    { name: 'organizationName', value: 'example.org' },
    { shortName: 'OU', value: 'Test' }
  ];
  cert.setSubject(attrs);
  cert.setIssuer(attrs);
  cert.setExtensions([{
    name: 'basicConstraints',
    cA: true
  }]);
  cert.sign(keys.privateKey, forge.md.sha256.create());
  return {
    privateKey: keys.privateKey,
    publicKey: keys.publicKey,
    cert: cert
  };
}

function generateCertificate(ca, commonName) {
  const keys = forge.pki.rsa.generateKeyPair(2048);
  const cert = forge.pki.createCertificate();
  cert.publicKey = keys.publicKey;
  cert.serialNumber = '02';
  cert.validity.notBefore = new Date();
  cert.validity.notAfter = new Date();
  cert.validity.notAfter.setFullYear(cert.validity.notBefore.getFullYear() + 1);
  const attrs = [
    { name: 'commonName', value: commonName }
  ];
  cert.setSubject(attrs);
  cert.setIssuer(ca.cert.subject.attributes);
  cert.sign(ca.privateKey, forge.md.sha256.create());
  return {
    privateKey: keys.privateKey,
    cert: cert
  };
}

function pemEncodeCert(cert) {
  return forge.pki.certificateToPem(cert);
}

function pemEncodeKey(key) {
  return forge.pki.privateKeyToPem(key);
}

function sendClientRequest() {
  const clientOptions = {
    hostname: 'localhost',
    port: port,
    path: '/',
    method: 'GET',
    key: clientKeyPem,
    cert: clientCertPem,
    ca: [caCertPem],
    rejectUnauthorized: false
  };

  const req = https.request(clientOptions, (res) => {
    let data = '';
    res.on('data', (chunk) => {
      data += chunk;
    });
    res.on('end', () => {
      console.log('Response from server:', data);
    });
  });

  req.on('error', (e) => {
    console.error('errp', e);
  });

  req.end();
}
