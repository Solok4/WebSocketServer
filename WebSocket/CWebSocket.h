#pragma once
#include "SocketWrapper/SocketWrap.h"
#include "SHA1.h"
#include "Base64.h"
#include <stdint.h>

#define MAX_SERVER_CLIENTS 16

enum WebSocketCloseReason
{
	NORMAL = 1000,
	GOING_AWAY,
	PROTOCOL_ERROR,
	WRONG_DATA_TYPE,
	NON_CONSISTENT_TYPE_OF_MESSAGE = 1007,
	POLICY_VIOLATION,
	TOO_BIG_MESSAGE,
	UNEXPECTED_CONDITION = 1011,

};

enum WebSocketOpcodes
{
	CONTINUATION=0x0,
	TEXT,
	BINARY,
	CLOSE =0x8,
	PING,
	PONG,

};


struct WebSocketFrameFormat
{
	bool FIN;
	bool RSV1;
	bool RSV2;
	bool RSV3;
	uint8_t Opcode : 4;
	bool MASK;
	uint8_t PayloadLen : 7;
	union {
		uint16_t ExtendedPayloadLength126;
		uint64_t ExtendedPayloadLength127;
	};
	char MaskingKey[4];
	char* PtrToData;

	void Decode(char* Buffer,const char* Message, uint64_t* EncodedLen);
	int Encode(char* Buffer,bool FinFlag, bool RSV1, bool RSV2, bool RSV3, uint8_t Opcode, bool Mask, uint64_t PayloadLen);
};

struct WebSocketInstance
{
	std::shared_ptr<SocketInstance> Socket;
};

struct EssentialInfo
{
	bool IsGet =false;
	char Resource[64] = { '\0' };
	char Host[16] = {'\0'};
	char UserAgent[256] = { '\0' };
	char Origin[256] = { '\0' };
	char WebSocketKey[128] = { '\0' };
	char WebSocketVersion[64] = { '\0' };
	char Connection[128] = { '\0' };
	char Upgrade[128] = { '\0' };
};

class CWebSocket
{
public:
	CWebSocket();

	bool CreateWebSocketServer(bool isSecure);
	void CloseWebSocketServer();
	void BindRecieveFunction(std::function<void(std::shared_ptr<WebSocketInstance>, char*, uint64_t)>);
	~CWebSocket();

	void SendWebSocket(std::shared_ptr<WebSocketInstance>,const char*, uint64_t, WebSocketOpcodes);
	void CloseConnection(std::shared_ptr<WebSocketInstance>,WebSocketCloseReason);


	void RecieveWebSocket(std::shared_ptr<SocketInstance>, char*, int);
	void ProcessRequest(std::shared_ptr<SocketInstance>, char*, int);




private:
	SocketWrap SocketWrapper;
	std::shared_ptr<SocketInstance> ListeningSocket;
	std::vector<std::shared_ptr<SocketInstance>> ServerSocketInstances;
	std::vector<std::shared_ptr<WebSocketInstance>> WebSocketInstances;

	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> HandeFunc;
	std::function<void(std::shared_ptr<WebSocketInstance>, char*, uint64_t)> WebSocketRecieveFunc;
};

