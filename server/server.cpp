#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>

#include <WinSock2.h>
#include <ws2tcpip.h>  // для getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <mutex>

#include "EasyMenu.h"
#include "EasyLogs.h"
#include "ServerLogic.h"


//---------------------------------------------------------- main

int main() {
	std::cout << sizeof(MsgHead);

	while (true)
		Sleep(1000);

	EasyLogs logs("serv");
	if (logs.is_open() == false) {
		logs.create("serv");
		logs.insert(EL_SYSTEM, EL_ERROR, "Ошибка открытия логов (binary), создан новый");
	}

	SOCKET door_sock;

	if (SetupServer(door_sock, logs) == false) {
		logs.insert(EL_SYSTEM, EL_ERROR, "Неудачный запус сервера, сервер закрыт");
		logs.save();
		std::cout << "Неудачный запус сервера, сервер закрыт";

		return 0;
	}

	// (сокет сервера запущен и поставлен в прослушку)
	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер успешно запущен");

	ServerData server;
	
	std::thread door_thread(ServerMain, door_sock, logs, server);	// открыли поток
	
	ServerMenu(server);	// выполняем логику меню

	// закрытие всего
	server.set_state(-1);// дали команду закрыть сервер

	door_thread.join();	// ждем завершения потока двери

	WSACleanup();
	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер закрыт");
	std::cout << "сервер закрыт";
	logs.save();
}