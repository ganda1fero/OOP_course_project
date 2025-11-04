#include <iostream>
#include <string>
#include <chrono>

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#include "ServerLogic.h"
#include "EasyMenu.h"
#include "EasyLogs.h"
#include "ServerMenues.h"

//---------------------------------------------------------- main

int main() {
	// инициализация
	EasyLogs logs("serv");
	if (logs.is_open() == false) {
		logs.create("serv");
		logs.insert(EL_SYSTEM, EL_ERROR, "Ошибка открытия логов (binary), создан новый");
	}

	SOCKET door_sock;

	if (SetupServer(door_sock, logs) == false) {
		logs.insert(EL_SYSTEM, EL_ERROR, "Неудачный запуск сервера, сервер закрыт");
		logs.save();
		std::cout << "Неудачный запуск сервера, сервер закрыт";

		return 0;
	}

	time_t start_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	// (сокет сервера запущен и поставлен в прослушку)
	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер успешно запущен");

	ServerData server;
	server.ReadFromFile();

	logs.insert(EL_SYSTEM, "Найдено аккаунтов: " + std::to_string(server.get_all_account_notes().size()));
	
	std::thread door_thread(ServerMain, std::ref(door_sock), std::ref(logs), std::ref(server));	// открыли поток
	
	ServerMenu(server, logs);	// выполняем логику меню

	// закрытие всего
	server.set_state(-1);// дали команду закрыть сервер

	door_thread.join();	// ждем завершения потока двери

	server.SaveToFile();

	WSACleanup();

	time_t duration_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) - start_time_t;
	std::string duration_str{ std::to_string(duration_t / 3600) + "ч " + std::to_string((duration_t / 60) % 60) + "мин" };

	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер закрыт, время работы: " + duration_str);
	std::cout << "сервер закрыт";
	logs.save();
}