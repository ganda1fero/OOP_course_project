#include "ClientLogic.h"

// первый char код
#define FROM_SERVER 100	
#define FROM_CLIENT 55
// второй char код
#define ACCESS_DENIED 66

// методы классов

MsgHead::MsgHead() {
	first_code = UCHAR_MAX;
	second_code = UCHAR_MAX;
	third_code = UINT32_MAX;
	msg_length = 0;
}

int MsgHead::size_of() const {
	return 10;
}

bool MsgHead::read_from_char(const char* ptr) {
	uint32_t ptr_index = 0;
	try {
		first_code = *reinterpret_cast<const unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(first_code);

		second_code = *reinterpret_cast<const unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(second_code);

		third_code = *reinterpret_cast<const uint32_t*>(ptr + ptr_index);
		ptr_index += sizeof(third_code);

		msg_length = *reinterpret_cast<const uint32_t*>(ptr + ptr_index);
	}
	catch (...) {	// какая-то UB
		first_code = UCHAR_MAX;
		second_code = UCHAR_MAX;
		third_code = UINT32_MAX;
		msg_length = 0;

		return false;	// перевод неудачный
	}

	return true;	// удачный перевод
}

Client_data::Client_data(){
	state_ = 0;
	is_connected_ = false;
}

// функции

bool SetupClient(Client_data& client_data) {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA
		return false;	// выход
	}	// WSA запущен

	if ((client_data.door_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		// Ошибка открытия сокета
		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	return true;	// успешный запуск (+ подключение)
}

bool ConnectClient(Client_data& client_data) {
	{
		bool tmp_flag{ false }; 
		{
			std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
			tmp_flag = client_data.is_connected_;
		}
		if (tmp_flag == true) {
			client_data.state_mutex.lock();
			client_data.state_ = -1;	// закрытие соединения
			client_data.state_mutex.unlock();

			client_data.connect_thread.join();	// ждем завершение потока
		}
	}

	sockaddr_in server_adress;
	server_adress.sin_family = AF_INET;
	server_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE)
		server_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		server_adress.sin_addr.s_addr = inet_addr("26.225.195.119"); // IP адресс RadminVPN

	u_long mode = 0; // 0 = блокирующий режим
	ioctlsocket(client_data.door_sock, FIONBIO, &mode);	// поставили сокет в блокирующий режим (чисто для удобного connect)

	if (connect(client_data.door_sock, (sockaddr*)&server_adress, sizeof(server_adress)) == SOCKET_ERROR) {
		// ошибка подключения
		std::cout << "Не удалось подключиться! ";
		
		closesocket(client_data.door_sock);

		return false;
	}

	mode = 1; // 1 = неблокирующий режим
	ioctlsocket(client_data.door_sock, FIONBIO, &mode);	// поставили сокет в неблокирующий режим (асинхронный)
	
	{
		std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
		client_data.is_connected_ = true;
	}

	client_data.connect_thread = std::thread(ClientThread, std::ref(client_data));

	return true;
}

void ClientThread(Client_data& client_data) {
	char packet_buffer[1024];
	int32_t packet_size{ 0 };

	std::vector<char> recv_buffer;

	MsgHead msg_header;

	bool is_optimizated{ false };

	while (true) {	// основное тело

		packet_size = recv(client_data.door_sock, packet_buffer, 1024, 0);	// пытаемся прочитать

		if (packet_size > 0) {	// прочитали какие-то данные

			recv_buffer.insert(recv_buffer.begin(), packet_buffer, packet_buffer + packet_size);	// добавили в общий массив

			while (true) {	// пытаемся прочиать все возможные запросы
				if (recv_buffer.size() < msg_header.size_of())
					break;	// не хвататет даже на заголовок

				if (msg_header.read_from_char(&recv_buffer[0]) == false) {
					// ошибка, закрываем соединение
					{
						std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
						client_data.is_connected_ = false;
					}
					{
						std::lock_guard<std::mutex> lock(client_data.state_mutex);
						client_data.state_ = -1;
					}

					closesocket(client_data.door_sock);

					AuthorisationMenu(client_data, "Ошибка приема данных");

					return;
				}

				if (recv_buffer.size() < msg_header.size_of() + msg_header.msg_length)
					break;	// не хавтате чтобы прочиать 

				if (ProcessMessage(msg_header, recv_buffer, client_data) == false) {
					// ошибка, закрываем соединение
					{
						std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
						client_data.is_connected_ = false;
					}
					{
						std::lock_guard<std::mutex> lock(client_data.state_mutex);
						client_data.state_ = -1;
					}

					closesocket(client_data.door_sock);

					AuthorisationMenu(client_data, "Ошибка протокола/сети");

					return;
				}

				// очищаем использованные данные
				recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + msg_header.size_of() + msg_header.msg_length);
			}
		}
		else if (packet_size == 0) {	// сервер закрыл соединение через closesocket()
			break;
		}
		else if (WSAGetLastError() == WSAEWOULDBLOCK) {	// надо просто подождать
			Sleep(20);
			is_optimizated = true;
		}
		else {	// просто ошибка SOCKET_ERROR
			// закрываем соединение
			break;
		}

		{
			std::lock_guard<std::mutex> lock1(client_data.is_connected_mutex);
			std::lock_guard<std::mutex> lock2(client_data.state_mutex);

			if (client_data.is_connected_ == false || client_data.state_ == -1) {
				{
					std::lock_guard<std::mutex> lock1(client_data.is_connected_mutex);
					client_data.is_connected_ = false;
				}
				{
					std::lock_guard<std::mutex> lock2(client_data.state_mutex);
					client_data.state_ = -1;
				}

				closesocket(client_data.door_sock);

				AuthorisationMenu(client_data, "");

				return;
			}
			else if (is_optimizated == false)
				Sleep(20);
		}
	}

	// закрытие соединения
	{
		std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
		client_data.is_connected_ = false;
	}
	{
		std::lock_guard<std::mutex> lock(client_data.state_mutex);
		client_data.state_ = -1;
	}

	closesocket(client_data.door_sock);

	AuthorisationMenu(client_data, "");

	return;
}

bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	if (msg_header.first_code != FROM_SERVER)
		return false;

	switch (msg_header.second_code)
	{
	case ACCESS_DENIED:
		return AccessDenied(msg_header, recv_buffer, client_data);
		break;
	default:
		return false;	// неизвестная команда
		break;
	}

	return true;
}

bool AccessDenied(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// буферы
	unsigned char uchar_buffer{ 0 };
	uint32_t uint32_t_buffer{ 0 };
	std::string str_buffer;

	// указатель  
	uint32_t index = msg_header.size_of();

	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t_buffer);

		str_buffer.resize(uint32_t_buffer);

		str_buffer.insert(str_buffer.end(), &recv_buffer[index], &recv_buffer[index] + str_buffer.size());
	}
	catch (...) {	// какая-то ошибка
		return false;
	}

	AuthorisationMenu(client_data, str_buffer);

	return true;
}

void AuthorisationMenu(Client_data& client_data, std::string text) {
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	if (text.empty()) {
		client_data.menu_.set_info("Авторизация");
		client_data.menu_.set_info_main_color(DARK_GRAY_COLOR);
	}
	else {
		client_data.menu_.set_info(text);
		client_data.menu_.set_info_main_color(YELLOW_COLOR);
	}

	client_data.menu_.push_back_advanced_cin("Логин:");
	client_data.menu_.set_advanced_cin_new_allowed_chars(0, "1234567890");

	client_data.menu_.push_back_advanced_cin("Пароль:");
	client_data.menu_.set_advanced_cin_ban_not_allowed_on(1);
	client_data.menu_.set_advanced_cin_new_allowed_chars(1, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	client_data.menu_.set_advanced_cin_secure_input_on(1);

	client_data.menu_.push_back_butt("Войти");
	
	client_data.menu_.push_back_butt("Выход из программы");

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = AUTHORISATION_MENUTYPE;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = NO_ROLE;
}