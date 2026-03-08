const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const { spawn, execSync } = require('child_process');
const fs = require('fs');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static(__dirname));

['gs.txt', 'a.txt', 'b.txt', 'c.txt'].forEach(file => {
    if (!fs.existsSync(file)) fs.writeFileSync(file, ''); 
});

let activeProcesses = [];

// Ultimate Zombie Killer Function
function killZombies() {
    try {
        activeProcesses.forEach(p => { try { p.kill('SIGKILL'); } catch (e) {} });
        activeProcesses = [];
        // Forcefully execute Linux killall to guarantee ports are freed
        execSync('killall -9 sat_a sat_b sat_c ground_station 2>/dev/null');
    } catch (e) {
        // Safe to ignore: means no zombies were found to kill
    }
}

// Kill zombies immediately on server boot just in case
killZombies();

io.on('connection', (socket) => {
    socket.on('start_simulation', (data) => {
        
        // 1. Guarantee a clean slate before launching
        killZombies();

        // 2. Wipe the log files
        ['gs.txt', 'a.txt', 'b.txt', 'c.txt'].forEach(file => {
            fs.writeFileSync(file, ''); 
        });

        // 3. Spawn freshly
        const spawnUnbuffered = (cmd) => spawn(`stdbuf -o0 ${cmd}`, { shell: true });
        
        const satA = spawnUnbuffered('./sat_a');
        const satB = spawnUnbuffered('./sat_b');
        const satC = spawnUnbuffered('./sat_c');
        const gs   = spawnUnbuffered('./ground_station');
        
        activeProcesses.push(satA, satB, satC, gs);

        const pipeLog = (nodeName, process) => {
            process.stdout.on('data', (out) => {
                const lines = out.toString().split('\n');
                lines.forEach(line => {
                    if(line.trim() !== '') {
                        socket.emit('engine_log', { node: nodeName, msg: line });
                    }
                });
            });
        };

        pipeLog('sat_a', satA);
        pipeLog('sat_b', satB);
        pipeLog('sat_c', satC);
        pipeLog('gs', gs);

        // 4. Send inputs to GS
        setTimeout(() => {
            gs.stdin.write(`${data.dest}\n`);
            gs.stdin.write(`${data.route}\n`);
            gs.stdin.write(`${data.size}\n`);
            gs.stdin.write(`${data.payload}\n`);
            gs.stdin.write(`${data.bps}\n`);
            gs.stdin.write(`${data.time || "00:00:00:00"}\n`);
        }, 500);
    });

    // 5. Triggered by the Stop button OR auto-triggered on Cycle Complete
    socket.on('kill', () => {
        killZombies();
    });
    
    socket.on('disconnect', () => {
        killZombies();
    });
});

server.listen(3000, () => {
    console.log('DTN Unified Engine running on http://localhost:3000');
});