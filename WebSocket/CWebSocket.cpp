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
	Sleep(50);
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
	for (auto ws : this->WebSocketInstances)
	{
		this->CloseConnection(ws,WebSocketCloseReason::GOING_AWAY);
	}
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

void CWebSocket::SendWebSocket(std::shared_ptr<WebSocketInstance> WSoc, const char* data, uint64_t datalen, WebSocketOpcodes Opcode)
{
	WebSocketFrameFormat WSFormat;
	char SendBuffer[1024];
	int Headersize = WSFormat.Encode(SendBuffer, true, false, false, false,Opcode, false, datalen);
	char* PtrToSendBuffer = SendBuffer;
	PtrToSendBuffer += Headersize;
	strcat_s(PtrToSendBuffer,sizeof(SendBuffer)-Headersize,data);
	WSoc->Socket->SendTCPClient(SendBuffer, Headersize+datalen);
}

void CWebSocket::CloseConnection(std::shared_ptr<WebSocketInstance> WSoc,WebSocketCloseReason Reason)
{
	WebSocketFrameFormat WSFormat;
	char Buffer[64];
	int HeaderSize = WSFormat.Encode(Buffer, true, false, false, false, WebSocketOpcodes::CLOSE, false, 3);
	int SendBuffSize = HeaderSize;
	Buffer[SendBuffSize++]= Reason >> 8;
	Buffer[SendBuffSize++] = Reason &0xff;
	Buffer[SendBuffSize++] = '\0';

	WSoc->Socket->SendTCPClient(Buffer, SendBuffSize);
	for (int i = 0; i < this->WebSocketInstances.size(); i++)
	{
		if (this->WebSocketInstances[i]->Socket->GetSocket() == WSoc->Socket->GetSocket())
		{
			this->WebSocketInstances.erase(this->WebSocketInstances.begin() + i);
		}
	}
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
	char DecodedMessage[1024];
	uint64_t EncodedLen = 0;
	WSFormat.Decode(DecodedMessage,data, &EncodedLen);

	this->WebSocketRecieveFunc(WSInstance, DecodedMessage, EncodedLen);
}

void WebSocketFrameFormat::Decode(char* Buffer,const char* Message,uint64_t* EncodedLen)
{
	byte SingleCharacter;
	*EncodedLen = 0;
	int ActualByte = 0;
	SingleCharacter = Message[ActualByte++];
	this->FIN = (SingleCharacter >> 7) & 0x1;
	this->RSV1 = (SingleCharacter >> 6) & 0x1;
	this->RSV2 = (SingleCharacter >> 5) & 0x1;
	this->RSV3 = (SingleCharacter >> 4) & 0x1;
	this->Opcode = SingleCharacter >> 3 & 0xF;
	SingleCharacter = Message[ActualByte++];
	this->MASK = (SingleCharacter >> 7) & 0x1;
	this->PayloadLen = SingleCharacter & 0x7F;
	if (this->PayloadLen == 126)
	{
		uint16_t PayloadLen = Message[ActualByte] << 8 | Message[ActualByte + 1];
		this->ExtendedPayloadLength126 = PayloadLen;
		ActualByte += 2;
		*EncodedLen = this->ExtendedPayloadLength126;
	}
	else if (this->PayloadLen == 127)
	{
		uint64_t PayloadLen = Message[ActualByte] << 56 | Message[ActualByte + 1] << 48 | Message[ActualByte + 2] << 40 | Message[ActualByte + 3] << 32 |
			Message[ActualByte + 4] << 24 | Message[ActualByte + 5] << 16 | Message[ActualByte + 6] << 8 | Message[ActualByte + 7];
		this->ExtendedPayloadLength127 = PayloadLen;
		ActualByte += 8;
		*EncodedLen = this->ExtendedPayloadLength127;
	}
	else
	{
		*EncodedLen = this->PayloadLen;
	}
	this->MaskingKey[0] = Message[ActualByte];
	this->MaskingKey[1] = Message[ActualByte + 1];
	this->MaskingKey[2] = Message[ActualByte + 2];
	this->MaskingKey[3] = Message[ActualByte + 3];
	ActualByte += 4;

	for (int i = 0; i < *EncodedLen; i++)
	{
		Buffer[i] = Message[ActualByte++] ^ this->MaskingKey[i % 4];
	}
}

int WebSocketFrameFormat::Encode(char* Buffer, bool FinFlag, bool RSV1, bool RSV2, bool RSV3, uint8_t Opcode, bool Mask, uint64_t PayloadLen)
{
	char* PtrToBuffer;
	char SingleChar;
	int HeaderSize = 0;

	PtrToBuffer = Buffer;
	SingleChar = FinFlag << 7;
	SingleChar += RSV1 << 6;
	SingleChar += RSV2 << 5;
	SingleChar += RSV3 << 4;
	SingleChar += Opcode;
	*PtrToBuffer = SingleChar;
	PtrToBuffer += 1;
	HeaderSize += 1;
	
	SingleChar = Mask << 7;
	if (PayloadLen <= 125)
	{
		SingleChar += PayloadLen;
		*PtrToBuffer = SingleChar;
		PtrToBuffer += 1;
		HeaderSize += 1;
	}
	else if ((PayloadLen > 125) && (PayloadLen < 0xffff))
	{
		SingleChar += 126;
		*PtrToBuffer = SingleChar;
		*(PtrToBuffer + 1) = PayloadLen >> 8;
		*(PtrToBuffer + 2) = PayloadLen &0xff;
		PtrToBuffer += 3;
		HeaderSize += 3;
	}
	else
	{
		SingleChar += 127;
		*PtrToBuffer = SingleChar;
		*(PtrToBuffer + 1) = (PayloadLen >> 56) & 0xff;
		*(PtrToBuffer + 2) = (PayloadLen >> 48) & 0xff;
		*(PtrToBuffer + 3) = (PayloadLen >> 40) & 0xff;
		*(PtrToBuffer + 4) = (PayloadLen >> 32) & 0xff;
		*(PtrToBuffer + 5) = (PayloadLen >> 24) & 0xff;
		*(PtrToBuffer + 6) = (PayloadLen >> 16) & 0xff;
		*(PtrToBuffer + 7) = (PayloadLen >> 8 ) & 0xff;
		*(PtrToBuffer + 8) = PayloadLen & 0xff;

		PtrToBuffer += 9;
		HeaderSize += 9;
	}
	*PtrToBuffer = '\0';

	return HeaderSize;
}
