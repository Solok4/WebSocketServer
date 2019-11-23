#include "SocketWrap.h"
#include <string>


SocketInstance::~SocketInstance()
{
	if (this != nullptr) {
		if (this->_Address != nullptr)
		{
			//freeaddrinfo(this->_Address);
		}
		delete this->_Name;
	}
}

void SocketInstance::SetupServer(const char* port)
{
	this->_SocType = SocketType::Server;
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	if(this->IpProto == IPPROTO_TCP)
		hints.ai_socktype = SOCK_STREAM;
	else
		hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status = getaddrinfo(NULL, port, &hints, &res) != 0))
	{
		Clog::Log(LogTag::Error, "%s > GetAddrInfo error:%s", this->_Name, gai_strerror(status));
		return;
	}
	else
	{
		for (addrinfo* p = res; p != NULL; p = p->ai_next)
		{
			void* addr;
			char ipstr[INET_ADDRSTRLEN];
			struct sockaddr_in* socka = (struct sockaddr_in*)p->ai_addr;
			addr = &(socka->sin_addr);
			inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
			//Clog::Log(LogTag::Info, "%s", ipstr);
		}
		this->_Address = res;
		if (this->IpProto == IPPROTO_TCP)
			this->_Address->ai_protocol = IPPROTO_TCP;
		else
			this->_Address->ai_protocol = IPPROTO_UDP;
		this->_Status = SocketStatus::HasAddresInfo;
	}
	if (this->_Status == SocketStatus::HasAddresInfo && this->_SocType == SocketType::Server)
	{
		if (status = bind(this->_Soc, this->_Address->ai_addr, this->_Address->ai_addrlen) != 0)
		{
#ifdef _WIN32
			Clog::Log(LogTag::Error, "%s > Server bind error: %d", this->_Name, WSAGetLastError());
#else
			Clog::Log(LogTag::Error, "%s > Server bind error", this->_Name);
#endif // _WIN32
			return;
		}
		else
		{
			Clog::Log(LogTag::Info, "%s > Server has been binded", this->_Name);
			this->_Status = SocketStatus::Binded;
			this->_Port = port;
		}

	}

}

void SocketInstance::CreateListeningThread(int maxClients)
{
	if (this->ListeningThread.native_handle() == NULL && this->_SocType == SocketType::Server && this->IpProto == IPPROTO_TCP)
		this->ListeningThread = std::thread(&SocketInstance::ListenServerFunc, this, maxClients);
	else if (this->ListeningThread.native_handle() == NULL && this->_SocType == SocketType::Server && this->IpProto == IPPROTO_UDP)
		this->ListeningThread = std::thread(&SocketInstance::RecieveFunc, this);
}

void SocketInstance::SendToAllClients(void* data, size_t size)
{
	for (auto a : this->ServerSockets)
	{
		a->SendTCPClient(data, size);
	}
}

std::vector<std::shared_ptr<SocketInstance>> SocketInstance::GetServerSockets()
{
	return this->ServerSockets;
}

void SocketInstance::RecieveFunc()
{
	fd_set readfds;
	SOCKET activity;
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int Length;
	char Buffer[8192];
	this->isListening = true;
	while ((this->isListening.load() == true && this->_Status == SocketStatus::Connected) || (this->IpProto == IPPROTO_UDP && this->isListening.load() == true))
	{
		FD_ZERO(&readfds);
		FD_SET(this->_Soc, &readfds);

		activity = select(this->_Soc, &readfds, NULL, NULL, &tv);
		if (activity < 0)
		{
			Clog::Log(LogTag::Error, "%s > Activity error", this->_Name);
		}
		if (FD_ISSET(this->_Soc, &readfds))
		{
			if (this->_Address->ai_protocol == IPPROTO_TCP)
			{
				Length = recv(this->_Soc, Buffer, sizeof(Buffer), 0);
				if (Length > 0)
				{
					//Data Recieved
					std::shared_ptr<SocketInstance> temp = std::make_shared<SocketInstance>(*this);
					this->ResponseFunction(temp, Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					Clog::Log(Info, "%s > Connection closed", this->_Name);
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}
				else
				{
					//Handle Error
#ifdef _WIN32
					Clog::Log(LogTag::Error, "%s > Failed recieving data in client listening func Error: %d", this->_Name, WSAGetLastError());
#else
					Clog::Log(LogTag::Error, "%s > Failed recieving data in client listening func Error", this->_Name);
#endif // _WIN32
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}
			}
			else if (this->_Address->ai_protocol == IPPROTO_UDP)
			{
#ifdef _WIN32
				int FromLen = this->_Address->ai_addrlen;
#else
				socklen_t FromLen = this->_Address->ai_addrlen;
#endif // _WIN32
				Length = recvfrom(this->_Soc, Buffer, sizeof(Buffer), 0, this->_Address->ai_addr, &FromLen);
				//Clog::Log(LogTag::Info, "Length: %d", Length);
				if (Length > 0)
				{
					//Data Recieved
					this->ResponseFunction(std::make_shared<SocketInstance>(*this), Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					this->isListening.store(false);
				}
				else
				{
					//Handle Error
#ifdef _WIN32
					Clog::Log(LogTag::Error, "%s > Failed recieving data in client listening func Error: %d", this->_Name, WSAGetLastError());
#else
					Clog::Log(LogTag::Error, "%s > Failed recieving data in client listening func Error", this->_Name);
#endif // _WIN32
					this->isListening.store(false);
				}
			}
		}
	}
	Clog::Log(LogTag::Info,  "%s > Stopping recieving thread",this->_Name);
}

void SocketInstance::ListenServerFunc(int maxClients)
{
	if (this->_SocType == SocketType::Server && this->_Status == SocketStatus::Binded)
	{
		fd_set readfds;
		SOCKET maxSd = INVALID_SOCKET, sd = INVALID_SOCKET, activity, newSock = INVALID_SOCKET;
		int addrLen = sizeof(this->_Address);
		char Buffer[8192];
		timeval tv;
		tv.tv_sec = 1;

		/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		Need to be fixed
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

		for (int i = this->ServerSockets.size(); i < maxClients; i++)
		{
			char ChildName[32];
			snprintf(ChildName,32, "%s %d", this->_Name, i);
			char* FinalName = new char[32];
			strcpy_s(FinalName,32, ChildName);
			auto child = this->_Reference->CreateEmptySocket(FinalName);
			child->_SocType = SocketType::Client;
			this->ServerSockets.push_back(child);
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (listen(this->_Soc, maxClients) == SOCKET_ERROR)
		{
#ifdef _WIN32
			Clog::Log(LogTag::Error, "%s > Socket failed to listen Error: %d", this->_Name, WSAGetLastError());
#else
			Clog::Log(LogTag::Error, "%s > Socket failed to listen Error", this->_Name);
#endif // _WIN32
			return;
		}
		this->_Status = SocketStatus::Listening;

		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)this->_Address->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(this->_Address->ai_family, addr, ipstr, sizeof(ipstr));

		Clog::Log(LogTag::Info, "%s > Server listening at:", this->_Name, ipstr);
		this->GetLocalIp();
		this->isListening = true;
		while (this->isListening.load() == true && this->_Status == SocketStatus::Listening)
		{
			FD_ZERO(&readfds);
			FD_SET(this->_Soc, &readfds);
			maxSd = this->_Soc;
			for (int i = 0; i < this->ServerSockets.size(); i++)
			{
				sd = this->ServerSockets[i]->_Soc;
				if (sd > 0)
					FD_SET(sd, &readfds);
				if (sd > maxSd)
					maxSd = sd;
			}

			activity = select(maxSd + 1, &readfds, NULL, NULL, &tv);
			if (activity < 0)
			{
				Clog::Log(LogTag::Error, "%s > Activity error", this->_Name);
			}
			if (FD_ISSET(this->_Soc, &readfds))
			{
				Clog::Log(LogTag::Info, "%s > New connection", this->_Name);
				if ((newSock = accept(this->_Soc, NULL, NULL)) < 0)
				{
#ifdef _WIN32
					Clog::Log(LogTag::Error, "%s > Failed to accept Error: %d\n", this->_Name, WSAGetLastError());
#else
					Clog::Log(LogTag::Error, "%s > Failed to accept Error\n", this->_Name);
#endif // _WIN32
					return;
				}

				// Welcome message
				/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
				{
					Clog:Log(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
				}*/


				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					if (this->ServerSockets[i]->_Soc == 0)
					{
						struct sockaddr addr;
						struct addrinfo hints, * res;

						this->ServerSockets[i]->_Soc = newSock;
#ifdef _WIN32
						int AddressSize = sizeof(addr);
#else
						socklen_t AddressSize = sizeof(addr);
#endif // _WIN32
						getpeername(this->ServerSockets[i]->_Soc, (struct sockaddr*) & addr, &AddressSize);

						memset(&hints, 0, sizeof(hints));
						hints.ai_family = AF_INET;
						hints.ai_socktype = SOCK_STREAM;

						void* addres;
						char ipstr[INET_ADDRSTRLEN];
						struct sockaddr_in* socka = (struct sockaddr_in*) & addr;
						addres = &(socka->sin_addr);
						inet_ntop(AF_INET, addres, ipstr, sizeof(ipstr));

						getaddrinfo(ipstr, this->_Port, &hints, &res);

						this->ServerSockets[i]->_Address = res;
						this->ServerSockets[i]->_Address->ai_protocol = IPPROTO_TCP;
						this->ServerSockets[i]->_Status = SocketStatus::Connected;
						std::shared_ptr<SocketInstance> inst(this->ServerSockets[i]);
						this->WelcomeFunction(inst, nullptr, 0);
						break;
					}
				}

			}
			else
			{
				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					sd = this->ServerSockets[i]->_Soc;
					if (FD_ISSET(sd, &readfds))
					{
						int valread;
						if ((valread = recv(sd, Buffer, sizeof(Buffer) / sizeof(Buffer[0]), 0)) == 0)
						{
							//Ending connection prompt
							void* addr;
							char ipstr[INET_ADDRSTRLEN];
							struct sockaddr_in* socka = (struct sockaddr_in*)this->ServerSockets[i]->_Address->ai_addr;
							addr = &(socka->sin_addr);
							inet_ntop(this->ServerSockets[i]->_Address->ai_family, addr, ipstr, sizeof(ipstr));
							Clog::Log(LogTag::Info, "%s > Host disconected. IP: %s\n", this->_Name, ipstr);
							this->ServerSockets[i]->CloseConnection();
							this->ServerSockets[i]->_Soc = 0;
							this->ServerSockets[i]->_Status = SocketStatus::Created;
						}
						else if (valread > 0)
						{
							//Process recieved data
							std::shared_ptr<SocketInstance> inst(this->ServerSockets[i]);
							this->ResponseFunction(inst, Buffer, valread);
						}
					}
				}
			}
		}
		Clog::Log(LogTag::Info, "%s > Stoping listening", this->_Name);
		for (auto i : this->ServerSockets)
		{
			if (i->_Soc != 0)	i->CloseConnection();
		}
	}
	else
	{
		Clog::Log(LogTag::Error, "%s > Server is not prepared for listening", this->_Name);
	}
}

void SocketInstance::SetSockType(int ipproto)
{
	this->IpProto = ipproto;
}


void SocketInstance::SetupClient(const char* addr, const char* port)
{
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	if (this->IpProto == IPPROTO_TCP)
		hints.ai_socktype = SOCK_STREAM;
	else
		hints.ai_socktype = SOCK_DGRAM;
	this->_SocType = SocketType::Client;
	if ((status = getaddrinfo(addr, port, &hints, &res) != 0))
	{
		Clog::Log(LogTag::Error, "%s > GetAddrInfo error:%s", this->_Name, gai_strerror(status));
		return;
	}
	else
	{
		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)res->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
		//Clog::Log(LogTag::Log, "%s", ipstr);
		this->_Address = res;
		if (this->IpProto == IPPROTO_TCP)
			this->_Address->ai_protocol = IPPROTO_TCP;
		else
			this->_Address->ai_protocol = IPPROTO_UDP;
		this->_Status = SocketStatus::HasAddresInfo;
	}
}

void SocketInstance::ConnectToServer()
{
	if (this->_Status != SocketStatus::HasAddresInfo)
	{
		Clog::Log(LogTag::Error, "%s > You are alowed to connect to server after specifing its adress", this->_Name);
		return;
	}
	else
	{
		FD_SET Writable;
		timeval Tv;
		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)this->_Address->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(this->_Address->ai_family, addr, ipstr, sizeof(ipstr));


		Tv.tv_sec = 5;
		int status;
		if ((status = connect(this->_Soc, this->_Address->ai_addr, this->_Address->ai_addrlen)) != 0)
		{
			if(WSAGetLastError() != 10035)
				Clog::Log(LogTag::Error, "%s > Failed to connect to server %s", this->_Name, ipstr);
			else
			{
				while (status != 0)
				{
					FD_ZERO(&Writable);
					FD_SET(this->_Soc, &Writable);
					select(this->_Soc, NULL, &Writable, NULL, &Tv);
					if (FD_ISSET(this->_Soc,&Writable))
					{
						Clog::Log(LogTag::Info, "%s > Connected to %s", this->_Name, ipstr);
						this->_Status = SocketStatus::Connected;
						return;
					}
				}
					
			}

		}
		else
		{
			Clog::Log(LogTag::Info, "%s > Connected to %s", this->_Name, ipstr);
			this->_Status = SocketStatus::Connected;
		}
	}
}

void SocketInstance::SendTCPClient(void* data, size_t size)
{
	if (this->_Status == SocketStatus::Connected)
	{
		char* ptr = (char*)data;
		size_t length = size;
		size_t PartSize = 0;
		while (0 < length)
		{
			PartSize = send(this->_Soc, ptr, size, NULL);
			if (PartSize == SOCKET_ERROR)
			{
				Clog::Log(LogTag::Error, "%s > Failed to send data", this->_Name);
				return;
			}
			ptr += PartSize;
			length -= PartSize;
		}
		//Clog::Log(LogTag::Info, "%s > Sent %d bytes", this->_Name, size);
	}
	else
	{
		Clog::Log(LogTag::Error, "%s > You have to connect to server first.", this->_Name);
		return;
	}
}

void SocketInstance::RecieveTCPClient(void* dataBuffer, size_t bufferSize)
{
	if (this->_Status == SocketStatus::Connected)
	{
		int recievedBytes = recv(this->_Soc, (char*)dataBuffer, bufferSize, NULL);
		if (recievedBytes > 0)
		{
			Clog::Log(LogTag::Info, "%s > Recieved %d bytes.", recievedBytes, this->_Name);
		}
		else if (recievedBytes == 0)
		{
			Clog::Log(LogTag::Info, "%s > Connection closed", this->_Name);
			this->_Status = SocketStatus::HasAddresInfo;
		}
		else
		{
			Clog::Log(LogTag::Error, "%s > Failed to recieve data", this->_Name);
		}
	}
}

void SocketInstance::SendUDPClient(void* data, size_t size)
{
	if (this->_Status == SocketStatus::HasAddresInfo)
	{
		char* ptr = (char*)data;
		size_t length = size;
		size_t PartSize = 0;
		while (0 < length)
		{
			PartSize = sendto(this->_Soc, ptr, length, 0, (sockaddr*)this->_Address->ai_addr, sizeof(sockaddr));
			if (PartSize == SOCKET_ERROR)
			{
				Clog::Log(LogTag::Error, "%s > Failed to send data %d", this->_Name,WSAGetLastError());
				return;
			}
			ptr += PartSize;
			length -= PartSize;
		}
		Clog::Log(LogTag::Info, "%s > Sent %d bytes", this->_Name, size);
	}
	else
	{
		Clog::Log(LogTag::Error, "%s > You have to enter address first.", this->_Name);
		return;
	}
}

void SocketInstance::RecieveUDPClient(void* dataBuffer, size_t bufferSize)
{
	if (this->_Status == SocketStatus::HasAddresInfo)
	{
		int AddrLen = sizeof(this->_Address);
		int recievedBytes = recvfrom(this->_Soc, (char*)dataBuffer, bufferSize, 0, (sockaddr*)this->_Address, &AddrLen);
		if (recievedBytes > 0)
		{
			Clog::Log(LogTag::Info, "%s > Recieved %d bytes.", recievedBytes, this->_Name);
		}
		/*else if (recievedBytes == 0)
		{
			Clog::Log(LogTag::Info, "%s > Connection closed", this->_Name);
			this->_Status = SocketStatus::HasAddresInfo;
		}*/
		else
		{
			Clog::Log(LogTag::Error, "%s > Failed to recieve data", this->_Name);
		}
	}
}

void SocketInstance::CreateRecieveThread()
{
	if (this->ListeningThread.native_handle() == NULL)// && this->_SocType == SocketType::Client)
		this->ListeningThread = std::thread(&SocketInstance::RecieveFunc, this);
}

const char* SocketInstance::GetName()
{
	return this->_Name;
}

SOCKET SocketInstance::GetSocket()
{
	return this->_Soc;
}

SocketStatus SocketInstance::GetStatus()
{
	return this->_Status;
}

void SocketInstance::SetStatus(SocketStatus stat)
{
	this->_Status = stat;
}

std::vector<const char*> SocketInstance::GetLocalIp()
{
	char Address[128];
	std::vector<const char*> AdressList;
	if (gethostname(Address, sizeof(Address)) != 0)
	{
		Clog::Log(LogTag::Error,"Failed to get host name ");
	}
	hostent* HostAddr =  gethostbyname(Address);
	for (int i = 0; HostAddr->h_addr_list[i] != 0; i++)
	{
		struct in_addr addr;
		memcpy(&addr.S_un.S_addr, HostAddr->h_addr_list[i], sizeof(addr.S_un.S_addr));
		Clog::Log(LogTag::Info, "%s > %d: %s",this->_Name, i, inet_ntoa(addr));
		AdressList.push_back(inet_ntoa(addr));
	}
	return AdressList;
}

void SocketInstance::BindSocketFunction(std::function<void(std::shared_ptr<SocketInstance>, char*, int)> func, SocketFunctionTypes type)
{
	switch (type)
	{
	case Welcome:
		this->WelcomeFunction = func;
		break;
	case Response:
		this->ResponseFunction = func;
		break;
	default:
		break;
	}
}

void SocketInstance::StopListeningThread()
{
	this->isListening = false;
	this->ListeningThread.join();
}

void SocketInstance::CloseConnection()
{
	int result;
	if (this->_Status == SocketStatus::Connected)
	{
		Clog::Log(LogTag::Info, "%s > Socket Closing %d", this->_Name, this->_Soc);
		if (this->_Soc != 0)
			if ((result = shutdown(this->_Soc, SD_SEND)) == SOCKET_ERROR)
			{
				Clog::Log(LogTag::Error, "%s > Failed to close connection", this->_Name);
			}
			else
			{
				this->_Status = SocketStatus::HasAddresInfo;
			}
	}
	else if (this->_SocType == SocketType::Server && (this->_Status == SocketStatus::Binded ||this->_Status == SocketStatus::Listening || this->IpProto == IPPROTO_UDP))
	{
		Clog::Log(LogTag::Info, "%s > Server socket Closing %d", this->_Name, this->_Soc);
#ifdef _WIN32
		if(closesocket(this->_Soc)!=0)
#else
		if (close(this->_Soc) != 0)
#endif
		{
			Clog::Log(LogTag::Info, "%s > Failed closing server socket %d", this->_Name, this->_Soc);
		}
		this->_Status = SocketStatus::Created;
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SocketWrap::SocketWrap()
{
#ifdef WIN32
	int error = WSAStartup(MAKEWORD(2, 2), &this->_WsaData);
	if (error != NO_ERROR)
	{
	Clog::Log(LogTag::Error, "Failed to initalize WSA");
		WSACleanup();
		return;
	}
#endif // WIN
}


SocketWrap::~SocketWrap()
{
#ifdef _WIN32
	WSACleanup();
#endif // _WIN32
}

#ifndef _WIN32
std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(const char* name, int proto)
#else
std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(const char* name, IPPROTO proto)
#endif // !_WIN32
{
	SOCKET soc;
	int ReuseAddr;
	int ReuseAddrLen = sizeof(ReuseAddr);
	unsigned long NonBlocking;

	if (proto == IPPROTO_TCP)
	{
		soc = socket(AF_INET, SOCK_STREAM, proto);
	}
	else// if (proto == IPPROTO_UDP)
	{
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	}
	if (soc == INVALID_SOCKET)
	{
#ifdef _WIN32
		Clog::Log(LogTag::Error, "%s > Failed to create a socket. Error: %d", name, WSAGetLastError());
#else
		Clog::Log(LogTag::Error, "%s > Failed to create a socket", name);
#endif // _WIN32
		return nullptr;
	}
	char* FinalName = new char[32];
	strcpy_s(FinalName, 32, name);
	//strncpy(FinalName, name,32);

	//ReuseAddr = 1;
	//setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, (const char*)& ReuseAddr, ReuseAddrLen);

	NonBlocking = 1;
#ifdef _WIN32
	ioctlsocket(soc, FIONBIO, &NonBlocking);
#else
	int flags = fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK);
	if(flags) Clog::Log(LogTag::Error, "%s > Failed to set socket mode to non blocking", name);
#endif
	

	auto mySocket = std::make_shared<SocketInstance>(FinalName, soc, this);
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::CreateEmptySocket(const char* name)
{
	char* FinalName = new char[32];
	strcpy_s(FinalName, 32, name);
	//strncpy(FinalName, name,32);
	auto mySocket = std::make_shared<SocketInstance>(FinalName, this);
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::GetSocketByName(const char* name)
{
	for (auto a : this->_Sockets)
	{
		if (strcmp(a->GetName(), name) == 0)
		{
			return a;
		}
	}
	return nullptr;
}

void SocketWrap::CloseSocket(const char* name)
{
	for (int i = 0; i < this->_Sockets.size(); i++)
	{
		if (strcmp(this->_Sockets[i]->GetName(), name) == 0)
		{
			this->_Sockets.erase(this->_Sockets.begin() + i);
			return;
		}
	}
	Clog::Log(LogTag::Error, "Socket named %s not found", name);
}
