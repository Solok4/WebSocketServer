#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"Ws2_32.lib")
#endif

#include <vector>
#include "Log.h"
#include <memory>
#include <thread>
#include <future>



#ifndef _WIN32
typedef int SOCKET;
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#define INVALID_SOCKET (int)(~0)
#define SOCKET_ERROR -1
#define SD_SEND 0x01
#endif // !_WIN32

struct SocketCombo
{
	SOCKET soc;
	sockaddr_in AdressInfo;
};

struct MySocket
{
	SOCKET soc;
	const char* name;
	sockaddr_in MainSocket,RecieveSocket;
	std::function<void(int, char*, int)> ResponseFunction = [](int, char*, int) {};
	std::function<void(int, char*, int)> WelcomeFunction = [](int, char*, int) {};
	std::atomic<bool> Close;
	SocketCombo ChildSockets[8];
};

struct ThreadSocket
{
	std::shared_ptr<MySocket> Socket;
	std::thread thr;
	const char* name;
};

enum SocketType
{
	Unspec =0,
	Client,
	Server
};

enum SocketFunctionTypes
{
	Welcome = 0,
	Response
};

enum SocketStatus
{
	Created=0,
	HasAddresInfo,
	Binded,
	Connected,
	Listening,
};
class SocketWrap;

class SocketInstance
{
public:
	SocketInstance()
	{
		this->_Name = "";
		this->_Soc = 0;
		this->_Port = nullptr;
		this->_Address = nullptr;
		this->_Status = SocketStatus::Created;
		this->_SocType = Unspec;
		this->_Reference = nullptr;
		this->WelcomeFunction = [](std::shared_ptr<SocketInstance>, char*, int) {};
		this->ResponseFunction = [](std::shared_ptr<SocketInstance>, char*, int) {};
	};
	SocketInstance(const char* Name, SocketWrap* ref) :SocketInstance()
	{
		this->_Reference = ref;
		this->_Name = Name;
	};
	SocketInstance(const SocketInstance& soc)
	{
		this->_Port = soc._Port;
		this->_Address = soc._Address;
		this->_Soc = soc._Soc;
		this->_SocType = soc._SocType;
		this->_Status = soc._Status;
		this->WelcomeFunction = soc.WelcomeFunction;
		this->ResponseFunction = soc.ResponseFunction;
		this->_Reference = soc._Reference;
		char* dest = (char*)malloc(32);
		strcpy_s(dest, 32, soc._Name);
		this->_Name = dest;
		this->IpProto = soc.IpProto;
	}
	SocketInstance(const char* Name, int socket,SocketWrap* ref) :SocketInstance(Name,ref)
	{
		this->_Soc = socket;
		WSAPROTOCOL_INFO ProtoInfo;
		int ProtoInfoLen = sizeof(WSAPROTOCOL_INFO);
		getsockopt(this->_Soc, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)& ProtoInfo, &ProtoInfoLen);
		this->IpProto = ProtoInfo.iProtocol;
	};
	~SocketInstance();
	//Server things
	void SetupServer(const char* port);
	void CreateListeningThread(int maxClients);
	void SendToAllClients(void* data, size_t size);
	std::vector<std::shared_ptr<SocketInstance>> GetServerSockets();

	//Client things
	void SetupClient(const char* addr,const char* port);
	void ConnectToServer();
	void SendTCPClient(void* data, size_t size);
	void RecieveTCPClient(void* dataBuffer, size_t bufferSize);
	void SendUDPClient(void* data, size_t size);
	void RecieveUDPClient(void* dataBuffer, size_t bufferSize);
	void CreateRecieveThread();


	//Universal
	const char* GetName();
	SOCKET GetSocket();
	SocketStatus GetStatus();
	void SetStatus(SocketStatus stat);
	std::vector<const char*> GetLocalIp();
	void BindSocketFunction(std::function<void(std::shared_ptr<SocketInstance>, char*, int)> func, SocketFunctionTypes type);
	void StopListeningThread();
	void CloseConnection();


private:
	void RecieveFunc();
	void ListenServerFunc(int maxClients);
	void SetSockType(int ipproto);

	SocketWrap* _Reference;
	std::atomic<bool> isListening;
	std::vector<std::shared_ptr<SocketInstance>> ServerSockets;
	SOCKET _Soc;
	const char* _Name;
	struct addrinfo* _Address;
	SocketStatus _Status;
	SocketType _SocType;
	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> WelcomeFunction;
	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> ResponseFunction;
	std::thread ListeningThread;
	const char* _Port;
	int IpProto;
};

class SocketWrap
{
public:
	SocketWrap();
	~SocketWrap();

#ifndef _WIN32
	std::shared_ptr<SocketInstance> CreateSocket(const char* name, int proto);
#else
	std::shared_ptr<SocketInstance> CreateSocket(const char* name, IPPROTO proto);
#endif // !_WIN32
	std::shared_ptr<SocketInstance> CreateEmptySocket(const char* name);
	std::shared_ptr<SocketInstance> GetSocketByName(const char* name);
	void CloseSocket(const char* name);


private:
#ifdef _WIN32
	WSAData _WsaData;
#endif // !_WIN32
	std::vector<std::shared_ptr<SocketInstance>> _Sockets;
};
