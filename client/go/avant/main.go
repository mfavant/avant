package main

import (
	"avant/client"
	"avant/proto_res"
	"log"
	"strconv"

	"google.golang.org/protobuf/proto"
)

func main() {
	// RPC服务端地址
	clientAddr := "127.0.0.1:20023"
	rpcAddr := "127.0.0.1:20024"
	// 这个Go进程的ID
	appId := "0.0.0.999"

	// 初始化RPC客户端
	_, err := client.NewClient(rpcAddr,
		appId,
		func(client *client.Client) error {
			return client.SendHandshake()
		},
		func(client *client.Client, pkg *proto_res.ProtoPackage) error {
			log.Println("RPC 收到包 CMD =", pkg.Cmd)

			if pkg.Cmd == proto_res.ProtoCmd_PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE {
				var msg proto_res.ProtoIPCStreamAuthHandshake
				if err := proto.Unmarshal(pkg.Protocol, &msg); err != nil {
					log.Println("解析失败:", err)
				} else {
					log.Println("RPC握手成功 AppId:", string(msg.AppId))
				}
			}

			return nil
		})

	if err != nil {
		log.Fatalln("创建RPC client.Client 失败：", err)
	}

	// 模拟多个客户端连接普通业务接口
	for i := 0; i < 100; i++ {
		// 每个连接都开一个协程去处理
		go func(i int) {
			var count uint64 = 0 // 计数器
			_, err := client.NewClient(clientAddr,
				appId,
				func(client *client.Client) error {
					log.Println("有客户端连接从断开已经重连成功 ", i)

					// 直接开始发包
					msg := &proto_res.ProtoCSReqExample{
						TestContext: []byte("Hello from client " + strconv.Itoa(i)),
					}
					return client.Send(proto_res.ProtoCmd_PROTO_CMD_CS_REQ_EXAMPLE, msg)
				},
				func(client *client.Client, pkg *proto_res.ProtoPackage) error {
					// log.Println("客户端 收到包 CMD =", pkg.Cmd)
					count++
					if count%1000 == 0 {
						log.Println("客户端 ", i, " pingpong count ", count, " * 100")
					}

					if pkg.Cmd == proto_res.ProtoCmd_PROTO_CMD_CS_RES_EXAMPLE {
						var resMsg proto_res.ProtoCSResExample
						if err := proto.Unmarshal(pkg.Protocol, &resMsg); err != nil {
							log.Println("解析失败:", err)
						} else {
							// log.Println("接收ProtoCmd_PROTO_CMD_CS_RES_EXAMPLE TestContext: ", string(resMsg.TestContext))
						}

						// 接收到包后马上再发一个
						reqMsg := &proto_res.ProtoCSReqExample{
							TestContext: []byte("Hello from client " + strconv.Itoa(i)),
						}
						return client.Send(proto_res.ProtoCmd_PROTO_CMD_CS_REQ_EXAMPLE, reqMsg)
					}

					return nil
				})

			if err != nil {
				log.Fatalln("创建RPC client.Client 失败：", err)
			}
		}(i)
	}

	select {}
}
