#include <iostream>

#include "ClientLogic.h"

int main() {
	// инициализация всего
	SOCKET door_sock;
	Client_data client_data;

	if (SetupClient(door_sock) == false) {
		std::cout << "Ошибка (SetupClient)";
		return 0;
	}

	// запуск работы
	ClientMenuLogic(door_sock, client_data);	// основная логика (меню)
	
	// завершение работы
	WSACleanup();
}