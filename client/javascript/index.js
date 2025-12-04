const protobuf = require("protobufjs")
const net = require("net")
const WebSocket = require("ws")
const dgram = require("dgram") // 引入 UDP 模块

const IP = "www.mfavant.xyz"
const PORT = 20025
const UDP_IP = "www.mfavant.xyz"
const UDP_PORT = 20027

const RPCIP = "www.mfavant.xyz"
const RPCPORT = 20026

const APPID = "0.0.0.369"

const IS_WEBSOCKET = true;
const IS_TCP = false;        // 为了测试清晰，我加了个开关控制 TCP
const IS_UDP = true;         // 开启 UDP 测试
const IS_TESTRPC = true;

function CreateAvantRPC(RPCIP, RPCPORT, protoRoot, OnRecvPackage) {
    let newAvantRPCObj = {
        client: null,
        recvBuffer: Buffer.alloc(0),
        OnRecvPackage,
        SendPackage(package) {
            if (!package) return;
            if (!this.client) {
                console.error("RPCObj Client is null");
                return;
            }
            const needSendBytes = ProtoPackage.encode(package).finish()
            const headLen = Buffer.alloc(8)
            {
                const uint64Value = BigInt(needSendBytes.length)
                const high = Number(uint64Value >> BigInt(32))
                const low = Number(uint64Value & BigInt(0xFFFFFFFF))
                headLen.writeUint32BE(high, 0)
                headLen.writeUint32BE(low, 4)
            }
            this.client.write(headLen)
            this.client.write(needSendBytes)
        },
        protoRoot,
        appId: null
    };

    const ProtoPackage = newAvantRPCObj.protoRoot.lookupType("ProtoPackage")
    const ProtoCmd = newAvantRPCObj.protoRoot.lookupEnum("ProtoCmd")

    const PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE = ProtoCmd.values['PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE']
    const ProtoIPCStreamAuthHandshake = newAvantRPCObj.protoRoot.lookupType("ProtoIPCStreamAuthHandshake")

    const PROTO_CMD_CS_REQ_EXAMPLE = ProtoCmd.values['PROTO_CMD_CS_REQ_EXAMPLE']
    const ProtoCSReqExample = newAvantRPCObj.protoRoot.lookupType("ProtoCSReqExample")
    const PROTO_CMD_CS_RES_EXAMPLE = ProtoCmd.values['PROTO_CMD_CS_RES_EXAMPLE']
    const ProtoCSResExample = newAvantRPCObj.protoRoot.lookupType("ProtoCSResExample")

    let tryConnect = () => {
        console.log(`tryConnect RPC ${RPCIP}:${RPCPORT}`)
        const client = net.createConnection({ port: RPCPORT, host: RPCIP }, () => {
            console.log("RPC Connected to server")

            const protoIPCStreamAuthHandshake = ProtoIPCStreamAuthHandshake.create({
                appId: Buffer.from(APPID, "utf8")
            });

            const reqPackage = ProtoPackage.create({
                cmd: PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE,
                protocol: ProtoIPCStreamAuthHandshake.encode(protoIPCStreamAuthHandshake).finish()
            });

            newAvantRPCObj.SendPackage(reqPackage);

            const protoCSReqExample = ProtoCSReqExample.create({
                testContext: Buffer.from(Date.now().toString(), "utf8")
            });

            const reqCSExample = ProtoPackage.create({
                cmd: PROTO_CMD_CS_REQ_EXAMPLE,
                protocol: ProtoCSReqExample.encode(protoCSReqExample).finish()
            });
            newAvantRPCObj.SendPackage(reqCSExample);
        });

        newAvantRPCObj.client = client;
        newAvantRPCObj.recvBuffer = Buffer.alloc(0)
        newAvantRPCObj.appId = null

        client.on('data', (data) => {
            if (data.length > 0) {
                newAvantRPCObj.recvBuffer = Buffer.concat([newAvantRPCObj.recvBuffer, data])
            }
            while (true) {
                if (newAvantRPCObj.recvBuffer.length <= 8) return;

                const high = newAvantRPCObj.recvBuffer.readUint32BE(0)
                const low = newAvantRPCObj.recvBuffer.readUint32BE(4)
                const uint64 = Number(BigInt(high) << BigInt(32) | BigInt(low))
                if (newAvantRPCObj.recvBuffer.length < 8 + uint64) return;

                let packageData = newAvantRPCObj.recvBuffer.subarray(8, 8 + uint64);
                newAvantRPCObj.recvBuffer = newAvantRPCObj.recvBuffer.subarray(8 + uint64);

                try {
                    const recvPackageData = ProtoPackage.decode(packageData)
                    if (recvPackageData.cmd == PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE) {
                        const ptotoIPCStreamAuthhandshake = ProtoIPCStreamAuthHandshake.decode(recvPackageData.protocol)
                        const appIdString = ptotoIPCStreamAuthhandshake.appId.toString('utf8')
                        console.log("appIdString ", appIdString)
                        newAvantRPCObj.appId = appIdString
                    }
                    else if (recvPackageData.cmd == PROTO_CMD_CS_RES_EXAMPLE) {
                        const protoCSResExample = ProtoCSResExample.decode(recvPackageData.protocol);
                        const testContextStr = protoCSResExample.testContext.toString('utf8');
                        const sendTime = parseInt(testContextStr);
                        const rttMs = !isNaN(sendTime) ? Date.now() - sendTime : 'N/A';
                        console.log(`[RPC] protoCSResExample from ${newAvantRPCObj.appId} testContext "${testContextStr}" RTT: ${rttMs}ms`);
                    }

                    if (newAvantRPCObj.OnRecvPackage) {
                        newAvantRPCObj.OnRecvPackage(newAvantRPCObj, recvPackageData);
                    }
                } catch (err) {
                    console.log(err.message)
                }
            }
        });

        client.on('end', () => {
            console.log('RPC end');
            setTimeout(tryConnect, 1000);
        });

        client.on('error', (err) => {
            console.log(err.message)
            setTimeout(tryConnect, 1000);
        });
    }

    tryConnect();
    return newAvantRPCObj;
}

function loadProtobuf() {
    return new Promise((resolve, reject) => {
        protobuf.load(
            [
                '../../protocol/proto_cmd.proto',
                '../../protocol/proto_example.proto',
                '../../protocol/proto_ipc_stream.proto',
                '../../protocol/proto_lua.proto',
                '../../protocol/proto_message_head.proto',
                '../../protocol/proto_tunnel.proto'
            ],
            (err, root) => {
                if (err) reject(err)
                else resolve(root)
            }
        );
    });
}

loadProtobuf().then(root => {
    const ProtoCmd = root.lookupEnum("ProtoCmd")
    const ProtoPackage = root.lookupType("ProtoPackage")
    const ProtoCSReqExample = root.lookupType("ProtoCSReqExample")
    const PROTO_CMD_CS_REQ_EXAMPLE = ProtoCmd.values['PROTO_CMD_CS_REQ_EXAMPLE']
    const ProtoCSResExample = root.lookupType("ProtoCSResExample")
    const PROTO_CMD_CS_RES_EXAMPLE = ProtoCmd.values['PROTO_CMD_CS_RES_EXAMPLE']

    // 创建一个辅助函数，每次调用都生成新的 ProtoCSReqExample 和对应的字节数据
    const createCSReqExamplePackage = () => {
        const csReqExample = ProtoCSReqExample.create({
            testContext: Buffer.from(Date.now().toString(), "utf8")
        });

        const reqPackage = ProtoPackage.create({
            cmd: PROTO_CMD_CS_REQ_EXAMPLE,
            protocol: ProtoCSReqExample.encode(csReqExample).finish()
        });

        const needSendBytes = ProtoPackage.encode(reqPackage).finish();

        return needSendBytes;
    };

    // TCP 专用：创建带长度头的包
    const createTCPPackage = () => {
        const needSendBytes = createCSReqExamplePackage();

        const headLen = Buffer.alloc(8);
        const uint64Value = BigInt(needSendBytes.length);
        const high = Number(uint64Value >> BigInt(32));
        const low = Number(uint64Value & BigInt(0xFFFFFFFF));
        headLen.writeUint32BE(high, 0);
        headLen.writeUint32BE(low, 4);

        return { needSendBytes, headLen };
    };

    // ==========================================
    // =============== TCP 逻辑 =================
    // ==========================================
    if (IS_TCP) {
        let tcpQpsCounter = 0
        let tcpRttSum = 0
        let tcpRttCount = 0
        setInterval(() => {
            const avgRtt = tcpRttCount > 0 ? (tcpRttSum / tcpRttCount).toFixed(2) : 'N/A'
            console.log(`TCP QPS = ${tcpQpsCounter}, Avg RTT = ${avgRtt}ms`)
            tcpQpsCounter = 0
            tcpRttSum = 0
            tcpRttCount = 0
        }, 1000)

        const sendCSReqExample = (client) => {
            const { needSendBytes, headLen } = createTCPPackage();
            client.write(headLen)
            client.write(needSendBytes)
        };

        let doConnect = () => {
            console.log(`doConnect TCP Client ${IP}:${PORT}`)
            const client = net.createConnection({ port: PORT, host: IP }, () => {
                console.log("TCP Connected to server")
                for (let i = 0; i < 100; i++) {
                    sendCSReqExample(client)
                }
            });

            let clientRecvBuffer = Buffer.alloc(0)
            client.on('data', (data) => {
                if (data.length > 0) {
                    clientRecvBuffer = Buffer.concat([clientRecvBuffer, data])
                }
                while (true) {
                    if (clientRecvBuffer.length <= 8) return;

                    const high = clientRecvBuffer.readUint32BE(0)
                    const low = clientRecvBuffer.readUint32BE(4)
                    const uint64 = Number(BigInt(high) << BigInt(32) | BigInt(low))
                    if (clientRecvBuffer.length < 8 + uint64) return;

                    let packageData = clientRecvBuffer.subarray(8, 8 + uint64);
                    clientRecvBuffer = clientRecvBuffer.subarray(8 + uint64);

                    try {
                        const recvPackageData = ProtoPackage.decode(packageData)
                        if (recvPackageData.cmd == PROTO_CMD_CS_RES_EXAMPLE) {
                            tcpQpsCounter++

                            const protoCSResExample = ProtoCSResExample.decode(recvPackageData.protocol);
                            const testContextStr = protoCSResExample.testContext.toString('utf8');
                            const sendTime = parseInt(testContextStr);
                            const rttMs = !isNaN(sendTime) ? Date.now() - sendTime : 0;
                            if (rttMs > 0) {
                                tcpRttSum += rttMs
                                tcpRttCount++
                            }

                            sendCSReqExample(client)
                        } else {
                            console.log("unknow cmd", recvPackageData.cmd)
                        }
                    } catch (err) {
                        console.log(err.message)
                    }
                }
            });

            client.on('end', () => {
                console.log("client end");
                setTimeout(doConnect, 1000)
            });

            client.on('error', (err) => {
                console.log(err.message)
                setTimeout(doConnect, 1000)
            });
        }
        doConnect();
    }

    // ==========================================
    // ============= WebSocket 逻辑 =============
    // ==========================================
    if (IS_WEBSOCKET) {
        let wsQpsCounter = 0;
        let wsRttSum = 0;
        let wsRttCount = 0;
        setInterval(() => {
            const avgRtt = wsRttCount > 0 ? (wsRttSum / wsRttCount).toFixed(2) : 'N/A'
            console.log(`WebSocket QPS = ${wsQpsCounter}, Avg RTT = ${avgRtt}ms`)
            wsQpsCounter = 0
            wsRttSum = 0
            wsRttCount = 0
        }, 1000);

        let doConnectWebSocket = () => {
            const wsUrl = `ws://${IP}:${PORT}`
            console.log(`doConnectWebSocket Client ${wsUrl}`)

            try {
                const ws = new WebSocket(wsUrl);

                ws.on('open', () => {
                    console.log("WebSocket Connected to server")
                    for (let i = 0; i < 100; i++) {
                        // WS 通常直接发送 payload，不需要 8字节头部
                        const needSendBytes = createCSReqExamplePackage();
                        ws.send(needSendBytes)
                    }
                });

                ws.on('message', (data) => {
                    // console.log(`[WebSocket] Received data length: ${data.length} bytes, hex: ${Buffer.from(data).toString('hex')}`);
                    try {
                        const recvPackageData = ProtoPackage.decode(new Uint8Array(data));

                        if (recvPackageData.cmd == PROTO_CMD_CS_RES_EXAMPLE) {
                            wsQpsCounter++;

                            const protoCSResExample = ProtoCSResExample.decode(recvPackageData.protocol);

                            const testContextStr = Buffer.from(protoCSResExample.testContext).toString('utf-8');
                            const sendTime = parseInt(testContextStr);
                            const rttMs = Date.now() - sendTime;
                            if (!isNaN(rttMs) && rttMs > 0) {
                                wsRttSum += rttMs
                                wsRttCount++
                            }

                            const needSendBytes = createCSReqExamplePackage();
                            ws.send(needSendBytes)
                        } else {
                            console.log("[WebSocket] unknow cmd", recvPackageData.cmd)
                        }
                    } catch (err) {
                        console.log("[WebSocket] Decode error:", err.message)
                    }
                });

                ws.on('close', () => {
                    console.log("WebSocket closed");
                    setTimeout(doConnectWebSocket, 1000)
                });

                ws.on('error', (err) => {
                    console.log("[WebSocket] error:", err.message)
                    setTimeout(doConnectWebSocket, 1000)
                });

            } catch (err) {
                console.log("[WebSocket] Connection error:", err.message)
                setTimeout(doConnectWebSocket, 1000)
            }
        }
        doConnectWebSocket();
    }

    // ==========================================
    // =============== UDP 逻辑 =================
    // ==========================================
    if (IS_UDP) {
        let udpQpsCounter = 0;
        let udpRttSum = 0;
        let udpRttCount = 0;
        setInterval(() => {
            const avgRtt = udpRttCount > 0 ? (udpRttSum / udpRttCount).toFixed(2) : 'N/A'
            console.log(`UDP QPS = ${udpQpsCounter}, Avg RTT = ${avgRtt}ms`)
            udpQpsCounter = 0
            udpRttSum = 0
            udpRttCount = 0
        }, 1000);

        let doConnectUDP = () => {
            console.log(`Starting UDP Client to ${UDP_IP}:${UDP_PORT}`);

            // 创建 UDP v4 Socket
            const udpClient = dgram.createSocket('udp4');

            // 错误监听
            udpClient.on('error', (err) => {
                console.log(`[UDP] Server error:\n${err.stack}`);
                udpClient.close();
            });

            // 消息监听 (UDP 是基于包的，收到就是完整的一包，不需要像 TCP 那样处理粘包)
            udpClient.on('message', (msg, rinfo) => {
                // console.log(`[UDP] Received data length: ${msg.length} bytes, hex: ${Buffer.from(msg).toString('hex')}`);
                try {
                    // 注意：这里默认假设 UDP 像 WebSocket 一样，只传输 Protobuf Payload。
                    // 如果你的服务器 UDP 协议和 TCP 完全一致（也需要 8字节 长度头），
                    // 请在解析前先切掉前 8 字节: const realData = msg.subarray(8);

                    const recvPackageData = ProtoPackage.decode(msg);

                    if (recvPackageData.cmd == PROTO_CMD_CS_RES_EXAMPLE) {
                        udpQpsCounter++;

                        const protoCSResExample = ProtoCSResExample.decode(recvPackageData.protocol);
                        const testContextStr = protoCSResExample.testContext.toString('utf8');
                        const sendTime = parseInt(testContextStr);
                        const rttMs = !isNaN(sendTime) ? Date.now() - sendTime : 0;
                        if (rttMs > 0) {
                            udpRttSum += rttMs
                            udpRttCount++
                        }

                        // 收到回包后继续发送 (Ping-Pong)
                        // 1. 标准 UDP 发送 (不带 8字节 TCP 长度头)
                        const needSendBytes = createCSReqExamplePackage();
                        udpClient.send(needSendBytes, UDP_PORT, UDP_IP);
                    } else {
                        console.log("[UDP] unknown cmd", recvPackageData.cmd);
                    }
                } catch (err) {
                    console.log("[UDP] Decode error:", err.message);
                }
            });

            // 开始发送 (并发 100)
            console.log("UDP Start sending...");
            for (let i = 0; i < 100; i++) {
                // 1. 标准 UDP 发送
                const needSendBytes = createCSReqExamplePackage();
                udpClient.send(needSendBytes, UDP_PORT, UDP_IP, (err) => {
                    if (err) console.log("[UDP] Send error", err);
                });
            }
        };

        doConnectUDP();
    }

    if (IS_TESTRPC) {
        const RPCConn = CreateAvantRPC(RPCIP, RPCPORT, root, (rpcObj, package) => {
            console.log("RPC OnRecvPackage", `CMD = ${package.cmd} FROM APPID=${rpcObj.appId}`)
        });
        RPCConn.SendPackage();
    }

}).catch(err => {
    console.log("LoadProtobuf Err", err);
});
