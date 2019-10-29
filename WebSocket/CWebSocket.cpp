#include "CWebSocket.h"


CWebSocket::CWebSocket()
{
}

bool CWebSocket::CreateWebSocketServer(bool isSecure)
{
	ListeningSocket =  this->SocketWrapper.CreateSocket("WebSocket", IPPROTO_TCP);
	if(isSecure) ListeningSocket->SetupServer("443");
	else ListeningSocket->SetupServer("80");
	ListeningSocket->CreateListeningThread(MAX_SERVER_CLIENTS);
	Sleep(10);
	this->ServerSocketInstances = ListeningSocket->GetServerSockets();
	this->HandeFunc = [&](std::shared_ptr<SocketInstance> a, char* b, int c) {
		this->RecieveWebSocket(a, b, c);
		this->ProcessRequest(a, b, c);
	};
	this->ListeningSocket->BindSocketFunction(HandeFunc,SocketFunctionTypes::Response);
	return true;
}

void CWebSocket::CloseWebSocketServer()
{
	ListeningSocket->StopListeningThread();
	ListeningSocket->CloseConnection();
	this->SocketWrapper.CloseSocket("WebSocket");
}

void CWebSocket::BindRecieveFunction(std::function<void(std::shared_ptr<WebSocketInstance>, char* , uint64_t)> func)
{
	this->WebSocketRecieveFunc = func;
}


CWebSocket::~CWebSocket()
{
}

void CWebSocket::ProcessRequest(std::shared_ptr<SocketInstance> soc, char* data, int datalen)
{
	bool found = false;
	for (int i = 0; i < this->WebSocketInstances.size(); i++)
	{
		if (this->WebSocketInstances[i]->Socket->GetSocket() == soc->GetSocket())
		{
			int sock = this->WebSocketInstances[i]->Socket->GetSocket();
			int thissoc = soc->GetSocket();
			found = true;
			break;
		}
	}
	if (found) return;

	char* ptrToBeginingOfTheLine;
	char* ptrToEndLine;
	int lenOfTheLine = 0;
	char SingleLine[256];
	char* isFound;
	EssentialInfo ess;
	ptrToBeginingOfTheLine = data;
	for (;;)
	{
		ptrToEndLine = strchr(ptrToBeginingOfTheLine, '\n');
		if (ptrToEndLine == NULL)
		{
			break;
		}
		lenOfTheLine = ptrToEndLine - ptrToBeginingOfTheLine;
		if (lenOfTheLine == 0) break;
		if (lenOfTheLine > sizeof(SingleLine)) lenOfTheLine = sizeof(SingleLine);
		strncpy_s(SingleLine, ptrToBeginingOfTheLine, lenOfTheLine);
		if (!ess.IsGet)
		{
			isFound = strstr(SingleLine, "GET");
			if (isFound != NULL)
			{
				ess.IsGet = true;
				strncpy_s(ess.Resource, isFound+4, lenOfTheLine-13);
				//Clog::Log(LogTag::Info, "GetFound");
			}
		}
		if (strlen(ess.Host) == 0)
		{
			isFound = strstr(SingleLine, "Host:");
			if (isFound != NULL)
			{
				strncpy_s(ess.Host, isFound + 6, lenOfTheLine - 7);
			}
		}
		if (strlen(ess.Origin) == 0)
		{
			isFound = strstr(SingleLine, "Origin:");
			if (isFound != NULL)
			{
				strncpy_s(ess.Origin, isFound + 8, lenOfTheLine - 9);
			}
		}
		if (strlen(ess.Connection) == 0)
		{
			isFound = strstr(SingleLine, "Connection:");
			if (isFound != NULL)
			{
				strncpy_s(ess.Connection, isFound + 12, lenOfTheLine - 13);
			}
		}
		if (strlen(ess.UserAgent) == 0)
		{
			isFound = strstr(SingleLine, "User-Agent:");
			if (isFound != NULL)
			{
				strncpy_s(ess.UserAgent, isFound + 12, lenOfTheLine - 13);
			}
		}
		if (strlen(ess.WebSocketKey) == 0)
		{
			isFound = strstr(SingleLine, "Sec-WebSocket-Key:");
			if (isFound != NULL)
			{
				strncpy_s(ess.WebSocketKey, isFound + 19, lenOfTheLine - 20);
			}
		}

		if (strlen(ess.Upgrade) == 0)
		{
			isFound = strstr(SingleLine, "Upgrade:");
			if (isFound != NULL)
			{
				strncpy_s(ess.Upgrade, isFound + 9, lenOfTheLine - 10);
			}
		}

		if (strlen(ess.WebSocketVersion) == 0)
		{
			isFound = strstr(SingleLine, "Sec-WebSocket-Version:");
			if (isFound != NULL)
			{
				strncpy_s(ess.WebSocketVersion, isFound + 23, lenOfTheLine - 24);
			}
		}
		ptrToBeginingOfTheLine = ptrToEndLine + 1;
	}
	//Clog::Log(LogTag::Info, "After Processing" );
	
	char StringToBeHashed[128];
	//char SHA1HashedHexString[80];
	char SHA1HashedString[40];
	//strncpy_s(StringToBeHashed, "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 61);
	strncpy_s(StringToBeHashed, ess.WebSocketKey, strlen(ess.WebSocketKey));
	strcat_s(StringToBeHashed, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	SHA1(SHA1HashedString, StringToBeHashed, strlen(StringToBeHashed));
	/*for (size_t offset = 0; offset < 20; offset++) {
		sprintf_s((SHA1HashedHexString + (2 * offset)),41, "%02x", SHA1HashedString[offset] & 0xff);
	}*/
	//Clog::Log(LogTag::Info, "SHA1 Hashed: %s", SHA1HashedString);
	char Base64EncodedString[128];
	bintob64(Base64EncodedString, SHA1HashedString, strlen(SHA1HashedString));
	//Clog::Log(LogTag::Info, "Base64 encoded: %s", Base64EncodedString);

	char MessageToClient[1024];
	sprintf_s(MessageToClient, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", Base64EncodedString);
	soc->SendTCPClient(MessageToClient, strlen(MessageToClient));
	std::shared_ptr<WebSocketInstance> WSInstance = std::make_shared<WebSocketInstance>();
	if (this->WebSocketInstances.size() <= MAX_SERVER_CLIENTS)
	{
		WSInstance->Socket = soc;
		this->WebSocketInstances.push_back(WSInstance);
	}
}

void CWebSocket::CloseConnection(std::shared_ptr<WebSocketInstance> WSoc)
{
	WebSocketFrameFormat WSFormat;
	WSFormat.FIN = true;
	WSFormat.RSV1 = true;
	WSFormat.RSV2 = true;
	WSFormat.RSV3 = true;
	WSFormat.MASK = false;
	WSFormat.Opcode = 0x8;



	WSoc->Socket->SendTCPClient((void*)&WSFormat, sizeof(WSFormat));
}

void CWebSocket::RecieveWebSocket(std::shared_ptr<SocketInstance> soc, char* data, int length)
{
	std::shared_ptr<WebSocketInstance> WSInstance;
	for (int i = 0; i < this->WebSocketInstances.size(); i++)
	{
		if (this->WebSocketInstances[i]->Socket->GetSocket() == soc->GetSocket())
		{
			WSInstance = this->WebSocketInstances[i];
			break;
		}
	}
	if (WSInstance == nullptr) return;

	WebSocketFrameFormat WSFormat;
	uint64_t EncodedLen = 0;
	byte SingleCharacter;
	int ActualByte = 0;
	SingleCharacter = data[ActualByte++];
	WSFormat.FIN = (SingleCharacter >> 7) & 0x1;
	WSFormat.RSV1 = (SingleCharacter >> 6) & 0x1;
	WSFormat.RSV2 = (SingleCharacter >> 5) & 0x1;
	WSFormat.RSV3 = (SingleCharacter >> 4) & 0x1;
	WSFormat.Opcode = SingleCharacter >> 3 & 0xF;
	SingleCharacter = data[ActualByte++];
	WSFormat.MASK = (SingleCharacter >> 7) & 0x1;
	WSFormat.PayloadLen = SingleCharacter & 0x7F;
	if (WSFormat.PayloadLen == 126)
	{
		uint16_t PayloadLen = data[ActualByte] << 8 | data[ActualByte+1];
		WSFormat.ExtendedPayloadLength126 = PayloadLen;
		ActualByte += 2;
		EncodedLen = WSFormat.ExtendedPayloadLength126;
	}
	else if (WSFormat.PayloadLen == 127)
	{
		uint64_t PayloadLen = data[ActualByte] << 56 | data[ActualByte + 1] << 48 | data[ActualByte+2] << 40 | data[ActualByte + 3] << 32 |
								data[ActualByte+4] << 24 | data[ActualByte + 5] << 16 | data[ActualByte+6] << 8 | data[ActualByte + 7];
		WSFormat.ExtendedPayloadLength127 = PayloadLen;
		ActualByte += 8;
		EncodedLen = WSFormat.ExtendedPayloadLength127;
	}
	else
	{
		EncodedLen = WSFormat.PayloadLen;
	}
	WSFormat.MaskingKey[0] = data[ActualByte];
	WSFormat.MaskingKey[1] = data[ActualByte + 1];
	WSFormat.MaskingKey[2] = data[ActualByte + 2];
	WSFormat.MaskingKey[3] = data[ActualByte + 3];
	ActualByte += 4;

	char DecodedMessage[1024];
	for (int i = 0; i < EncodedLen; i++)
	{
		DecodedMessage[i] = data[ActualByte++] ^ WSFormat.MaskingKey[i % 4];
	}
	//Clog::Log(LogTag::Info, "%s sent > %.*s\n", soc->GetName(), (int)EncodedLen, DecodedMessage);

	this->WebSocketRecieveFunc(WSInstance, DecodedMessage, EncodedLen);
}
