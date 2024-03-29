// WebSocket.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "CWebSocket.h"

int main()
{
	CWebSocket soc;
	soc.CreateWebSocketServer(false);
	soc.BindRecieveFunction([&](std::shared_ptr<WebSocketInstance> a,char* b, uint64_t c) {
		Clog::Log(LogTag::Info, "Recieved from %s  > %.*s", a->Socket->GetName(), (int)c, b);
		soc.SendWebSocket(a, "TestMessage", 12, WebSocketOpcodes::TEXT);
		});
	Sleep(5000);
	soc.CloseWebSocketServer();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
