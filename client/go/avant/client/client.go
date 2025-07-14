package client

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"
	"sync"
	"time"

	"avant/proto_res"

	"google.golang.org/protobuf/proto"
)

type Client struct {
	addr        string
	appId       string
	OnPackage   func(client *Client, pkg *proto_res.ProtoPackage) error
	OnReconnect func(client *Client) error

	conn    net.Conn
	recvBuf []byte
	lock    sync.Mutex
}

func NewClient(addr string, appId string,
	onReconnect func(client *Client) error,
	onPkg func(client *Client, pkg *proto_res.ProtoPackage) error) (*Client, error) {

	client := &Client{
		addr:        addr,
		appId:       appId,
		OnPackage:   onPkg,
		OnReconnect: onReconnect,
		recvBuf:     make([]byte, 0),
	}
	// 为连接开一个协程去处理
	go client.connectLoop()
	return client, nil
}

func (client *Client) connectLoop() {
	for {
		conn, err := net.Dial("tcp", client.addr)
		if err != nil {
			log.Println("reconnect failed:", err)
			time.Sleep(2 * time.Second)
			continue
		}
		client.lock.Lock()
		client.conn = conn
		client.recvBuf = make([]byte, 0)
		client.lock.Unlock()

		if client.OnReconnect != nil {
			err = client.OnReconnect(client)
			if err != nil {
				log.Println("OnReconnect failed:", err)
				conn.Close()
				time.Sleep(2 * time.Second)
				continue
			}
		}

		client.recvLoop()

		client.lock.Lock()
		if client.conn != nil {
			client.conn.Close()
			client.conn = nil
		}
		client.lock.Unlock()

		time.Sleep(2 * time.Second)
	}
}

func (client *Client) SendHandshake() error {
	msg := &proto_res.ProtoIPCStreamAuthhandshake{
		AppId: []byte(client.appId),
	}
	return client.Send(proto_res.ProtoCmd_PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE, msg)
}

func (client *Client) Send(cmd proto_res.ProtoCmd, message proto.Message) error {
	protoData, err := proto.Marshal(message)
	if err != nil {
		return fmt.Errorf("marshal message failed: %w", err)
	}

	pkg := &proto_res.ProtoPackage{
		Cmd:      cmd,
		Protocol: protoData,
	}

	encoded, err := proto.Marshal(pkg)
	if err != nil {
		return fmt.Errorf("marshal package failed: %w", err)
	}
	head := make([]byte, 8) // 8字的包长
	length := uint64(len(encoded))
	binary.BigEndian.PutUint64(head, length)

	client.lock.Lock()
	defer client.lock.Unlock()
	if client.conn == nil {
		return fmt.Errorf("connection is nil")
	}

	_, err = client.conn.Write(append(head, encoded...))
	if err != nil {
		return fmt.Errorf("write failed: %w", err)
	}
	return nil
}

func (client *Client) recvLoop() {
	buf := make([]byte, 4096)
	for {
		n, err := client.conn.Read(buf)
		if err != nil {
			if err != io.EOF {
				log.Println("read error:", err)
			}
			return
		}

		client.recvBuf = append(client.recvBuf, buf[:n]...)

		for {
			if len(client.recvBuf) < 8 {
				break
			}

			total := int(binary.BigEndian.Uint64(client.recvBuf[:8]))

			if len(client.recvBuf) < 8+total {
				break
			}

			// 包体数据
			body := client.recvBuf[8 : 8+total]
			// 把开头的内容删掉
			client.recvBuf = client.recvBuf[8+total:]

			var pkg proto_res.ProtoPackage
			if err := proto.Unmarshal(body, &pkg); err != nil {
				log.Println("unmarshal error:", err)
				continue
			}
			if client.OnPackage != nil {
				client.OnPackage(client, &pkg)
			}
		}
	}
}
