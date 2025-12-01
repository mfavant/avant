const protobuf = require("protobufjs")
const net = require("net")

const IP = "127.0.0.1"
const PORT = 20025

const RPCIP = "127.0.0.1"
const RPCPORT = 20026

const APPID = "0.0.0.369"

function CreateAvantRPC(RPCIP, RPCPORT, protoRoot, OnRecvPackage) {
    let newAvantRPCObj = {
        client: null,
        recvBuffer: Buffer.alloc(0),
        OnRecvPackage,
        SendPackage(package) {
            if (!package) {
                return;
            }
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
    // handshake to remove , PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE
    const ProtoCmd = newAvantRPCObj.protoRoot.lookupEnum("ProtoCmd")
    const PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE = ProtoCmd.values['PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE']
    const ProtoIPCStreamAuthHandshake = newAvantRPCObj.protoRoot.lookupType("ProtoIPCStreamAuthHandshake")

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
        });

        newAvantRPCObj.client = client;
        newAvantRPCObj.recvBuffer = Buffer.alloc(0)
        newAvantRPCObj.appId = null

        client.on('data', (data) => {
            // decode
            if (data.length > 0) {
                newAvantRPCObj.recvBuffer = Buffer.concat([newAvantRPCObj.recvBuffer, data])
            }
            while (true) {
                if (newAvantRPCObj.recvBuffer.length <= 8) {
                    return;
                }
                const high = newAvantRPCObj.recvBuffer.readUint32BE(0)
                const low = newAvantRPCObj.recvBuffer.readUint32BE(4)
                const uint64 = Number(BigInt(high) << BigInt(32) | BigInt(low))
                if (newAvantRPCObj.recvBuffer.length < 8 + uint64) {
                    return;
                }

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
                if (err) {
                    reject(err)
                } else {
                    resolve(root)
                }
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

    const csReqExample = ProtoCSReqExample.create({
        testContext: Buffer.from("hello world 你好 中文", "utf8")
    });

    const reqPackage = ProtoPackage.create({
        cmd: PROTO_CMD_CS_REQ_EXAMPLE,
        protocol: ProtoCSReqExample.encode(csReqExample).finish()
    });

    const needSendBytes = ProtoPackage.encode(reqPackage).finish()
    const headLen = Buffer.alloc(8)
    {
        const uint64Value = BigInt(needSendBytes.length)
        const high = Number(uint64Value >> BigInt(32))
        const low = Number(uint64Value & BigInt(0xFFFFFFFF))
        headLen.writeUint32BE(high, 0)
        headLen.writeUint32BE(low, 4)
    }

    const sendCSReqExample = (client) => {
        client.write(headLen)
        client.write(needSendBytes)
    };

    let doConnect = () => {
        console.log(`doConnect Client ${IP}:${PORT}`)
        const client = net.createConnection({ port: PORT, host: IP }, () => {
            console.log("Connected to server")
            for (let i = 0; i < 100; i++) {
                sendCSReqExample(client)
            }
        });
        let clientRecvBuffer = Buffer.alloc(0)
        let pingPongCounter = 0;
        client.on('data', (data) => {
            if (data.length > 0) {
                clientRecvBuffer = Buffer.concat([clientRecvBuffer, data])
            }
            while (true) {
                if (clientRecvBuffer.length <= 8) {
                    return;
                }
                const high = clientRecvBuffer.readUint32BE(0)
                const low = clientRecvBuffer.readUint32BE(4)
                const uint64 = Number(BigInt(high) << BigInt(32) | BigInt(low))
                if (clientRecvBuffer.length < 8 + uint64) {
                    return;
                }

                let packageData = clientRecvBuffer.subarray(8, 8 + uint64);

                clientRecvBuffer = clientRecvBuffer.subarray(8 + uint64);

                try {
                    const recvPackageData = ProtoPackage.decode(packageData)
                    if (recvPackageData.cmd == PROTO_CMD_CS_RES_EXAMPLE) {
                        const csResExample = ProtoCSResExample.decode(recvPackageData.protocol)
                        pingPongCounter++
                        if (pingPongCounter % 1000 == 0) {
                            console.log(pingPongCounter.toString(), csResExample.testContext.toString('utf8'))
                        }
                    } else {
                        console.log("unknow cmd", recvPackageData.cmd)
                    }
                    sendCSReqExample(client)
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

    doConnect(); // mock client

    // mock rpc
    const RPCConn = CreateAvantRPC(RPCIP, RPCPORT, root, (rpcObj, package) => {
        console.log("RPC OnRecvPackage", `CMD = ${package.cmd} FROM APPID=${rpcObj.appId}`)
    });
    RPCConn.SendPackage();

}).catch(err => {
    console.log("LoadProtobuf Err", err);
});
