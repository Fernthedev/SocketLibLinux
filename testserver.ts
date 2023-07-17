import * as net from "node:net";

var HOST = '0.0.0.0';
var PORT = 3506;

net.createServer((sock) => {
    console.log('CONNECTED: ' + sock.remoteAddress +':'+ sock.remotePort);

    sock.on('data', function(data) {
        console.log('DATA ' + sock.remoteAddress + ': ' + data);

        // sock.write()
    });

    sock.on('close', function(data) {
        console.log('CLOSED: ' + sock.remoteAddress +' '+ sock.remotePort);
    });
}).listen(PORT, HOST);

console.log('Server listening on ' + HOST +':'+ PORT);