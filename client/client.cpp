#include <iostream>

#include "ClientLogic.h"

int main() {
	// инициализация всего
	SOCKET door_sock;
	Client_data client_data;
	client_data.ReadFromFile();

	if (SetupClient(client_data) == false) {
		std::cout << "Ошибка (SetupClient)";
		return 0;
	}

	// запуск работы
	ClientMenuLogic(client_data);	// основная логика (меню)
	
	// завершение работы
	WSACleanup();
}