const http = require('http');
const fs = require('fs').promises; // Use promises for cleaner async/await syntax
const url = require('url');
const path = require('path');

const staticRoot = async (staticPath, req, res) => {
    res.writeHead(200, 'OK');
    res.end();
    return;

    try {
        const pathObj = url.parse(req.url, true);
        const filePath = path.join(staticPath, pathObj.pathname === '/' ? 'index.html' : pathObj.pathname);

        const content = await fs.readFile(filePath, 'binary'); // Read file asynchronously
        res.writeHead(200, 'OK');
        res.write(content, 'binary');
        res.end();
    } catch (err) {
        // Handle errors gracefully, e.g., log the error and send a generic error response
        // console.error(err);
        res.writeHead(500, 'Internal Server Error');
        res.end('<h1>Internal Server Error</h1>');
    }
};

const server = http.createServer(async (req, res) => {
    await staticRoot('/avant_static', req, res); // Use await for asynchronous function call
});

server.listen(20025, '0.0.0.0', () => {
    console.log('http://localhost:20025');
});
