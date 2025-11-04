#include "ServerLogic.h"

//---------------------------------------------------------- функции

bool SetupServer(SOCKET& door_sock, EasyLogs& logs) {
	logs.insert(EL_SYSTEM, EL_NETWORK, "Запуск сервера...");

	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA
		logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Ошибка запуска WSA");
		return false;	// выход
	}	// WSA запущен

	if ((door_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		// Ошибка открытия сокета
		logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Ошибка открытия сокета (door_sock)");
		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	u_long mode = 1; // 1 = неблокирующий режим
	ioctlsocket(door_sock, FIONBIO, &mode);

	sockaddr_in door_adress;
	door_adress.sin_family = AF_INET;
	door_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE)
		door_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		door_adress.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(door_sock, (sockaddr*)&door_adress, sizeof(door_adress)) == SOCKET_ERROR) { // привязка сокета к IP:порту
		// не удалось привязать адресс к сокету
		logs.insert(EL_ERROR, EL_SYSTEM, EL_NETWORK, "Ошибка привязки IP к сокету");
		WSACleanup();

		return false;	// выход
	}	// адресс привязан к сокету

	{
		if (door_adress.sin_addr.s_addr == htonl(INADDR_ANY)) {
			// Получаем локальный IP
			char hostname[256];
			if (gethostname(hostname, sizeof(hostname)) == 0) {
				addrinfo hints{}, * info = nullptr;
				hints.ai_family = AF_INET; // IPv4 только
				if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
					for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
						sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
						std::string ip = inet_ntoa(addr->sin_addr);
						logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер слушает на IP: " + ip + ":60888");
					}
					freeaddrinfo(info);
				}
				else {
					logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Не удалось получить IP через getaddrinfo()");
				}
			}
			else {
				logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Не удалось получить имя хоста через gethostname()");
			}
		}
		else {
			// Привязка к конкретному IP
			std::string tmp_str = inet_ntoa(door_adress.sin_addr);
			logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер слушает на IP: " + tmp_str + ":60888");
		}
	}

	if (listen(door_sock, 10) == SOCKET_ERROR) {
		// не уадлось поставить сокет в прослушку
		logs.insert(EL_ERROR, EL_SYSTEM, EL_NETWORK, "Сокет не удалось постваить в прослушку (listen)");
		WSACleanup();

		return false;	// выход
	}	// поставили сокет режим слушания



	return true;
}

void ServerMain(SOCKET& door_sock, EasyLogs& logs, ServerData& server) {
	SOCKET connection;
	sockaddr_in connection_addr;
	int size_of_connection_addr = sizeof(connection_addr);

	while (true) {	// беск цикл (работа сервера)
		if (server.get_state() == -1)
			break;	// закрытие сервера

		connection = accept(door_sock, (sockaddr*)&connection_addr, &size_of_connection_addr); // неблокирующий accept

		if (connection == INVALID_SOCKET) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)  // просто нет новых клиентов - ждем
				Sleep(50);
			else	// значит какая-то ошибка!
				logs.insert(EL_ERROR, EL_NETWORK, "Ошибка установки соединения");
		}
		else {	// есть новое подключение
			{
				std::string clientIP = inet_ntoa(connection_addr.sin_addr);
				logs.insert(EL_NETWORK, "Установлено соединение с " + clientIP + ", число потоков: " + std::to_string(server.get_count_of_connections() + 1));
			}

			u_long mode = 1;
			ioctlsocket(connection, FIONBIO, &mode);	// сделали поток неблокирующим

			serv_connection* connection_ptr = server.add_new_connection(connection, connection_addr);	// добавили соединение в базу

			std::thread t(ServerThread, connection_ptr, std::ref(logs), std::ref(server));	// создали поток
			t.detach();	// отсоединили поток
		}
	}

	// остановка сервера
	logs.insert(EL_SYSTEM, EL_NETWORK, "Остановка сервера...");

	closesocket(door_sock);
	
	while (server.get_count_of_connections() > 0)
		Sleep(500);
	
	logs.insert(EL_NETWORK, "Все соединения закрыты");

	return;
}

void ServerThread(serv_connection* connection_ptr, EasyLogs& logs, ServerData& server) {	// тело самого клиент-сервера
	char packet_buffer[1024];
	std::vector<char> recv_buffer;

	int32_t packet_size{ 0 };

	MsgHead msg_header;

	bool is_last_optimizated{ false };

	while (true) {	// главное тело
		is_last_optimizated = false;

		// пытаемся прочитать
		packet_size = recv(connection_ptr->connection, packet_buffer, 1024, 0);	// читаем

		if (packet_size > 0) {	
			// читаем какие-то данные

			recv_buffer.insert(recv_buffer.end(), packet_buffer, packet_buffer + packet_size);	// вставили байты в общий буфер

			connection_ptr->last_action = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());	// обновили время последнего онлайна

			// пробуем прочесть все возможные сообщения
			while (true) {
				if (recv_buffer.size() < msg_header.size_of())
					break;	// байтов не хватает даже на заголовок

				if (msg_header.read_from_char(&recv_buffer[0]) == false) {	// ошибка header
					std::string ip_str = inet_ntoa(connection_ptr->connection_addr.sin_addr);
					std::string tmp_str{ "Ошибка заголовка сообщения от" + ip_str};
					if (connection_ptr->account_ptr != nullptr)
						tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
					tmp_str += ". Закрываю соединение";
					logs.insert(EL_ERROR, EL_NETWORK, tmp_str);

					closesocket(connection_ptr->connection);
					server.del_connection(connection_ptr->connection);
					return;
				}

				if (recv_buffer.size() < msg_header.size_of() + msg_header.msg_length)
					break;	// байт недостаточно для сообщения
					
				if (ProcessMessage(msg_header, recv_buffer, connection_ptr, server, logs) == false) { // необходимо закрыть соединение
					// ошибку логирует Process
					closesocket(connection_ptr->connection);
					server.del_connection(connection_ptr->connection);
					return;
				}

				// очищаем использованные данные
				recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + msg_header.size_of() + msg_header.msg_length);
			}
		}
		else if (packet_size == 0) {
			// клиент закрыл соединение
			std::string ip_str = inet_ntoa(connection_ptr->connection_addr.sin_addr);
			std::string tmp_str{ "Пользователь " + ip_str };
			if (connection_ptr->account_ptr != nullptr)
				tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
			tmp_str += " закрыл соединение";
			logs.insert(EL_NETWORK, tmp_str);

			break;
		}
		else if (WSAGetLastError() == WSAEWOULDBLOCK) {	// скорее всего -1, но просто ничего не пришло
			is_last_optimizated = true;
			Sleep(20);
		}
		else { // ошибка
			std::string ip_str = inet_ntoa(connection_ptr->connection_addr.sin_addr);
			std::string	tmp_str{ "Ошибка приема данных от пользователя " + ip_str + " закрываю соединение"};
			if (connection_ptr->account_ptr != nullptr)
				tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
			logs.insert(EL_ERROR, EL_NETWORK, tmp_str);

			break;
		}

		// проверка на закрытие сервера
		if (server.get_state() == -1) {
			// необходимо закрыть соединение

			std::vector<char> message_data;

			CreateAccessDeniedMessage(message_data, "Сервер закрывается...");

			SendTo(connection_ptr, message_data, logs);	// отправили сообщение

			Sleep(200); // на всякий даем 200мс чтобы сообщение точно дошло
			
			break;	// закрываем соединение
		}
		else if (is_last_optimizated == false)
			Sleep(20);
	}

	// закрытие соединения (потока)
	closesocket(connection_ptr->connection);
	server.del_connection(connection_ptr->connection);
	return;
}

bool SendTo(serv_connection* connect_ptr, const std::vector<char>& data, EasyLogs& logs) {
	uint32_t sended_count{ 0 };

	int32_t tmp_count{ 0 };

	auto last_send_time{ std::chrono::steady_clock::now() };

	while (sended_count < data.size()) {	// выполняем пока количество реально отправленных меньше полной даты
		tmp_count = send(connect_ptr->connection, &data[sended_count], data.size() - sended_count, 0);

		if (tmp_count > 0) {
			// значит что-то уже отправилось
			sended_count += tmp_count;	// добавили счетчик
			last_send_time = std::chrono::steady_clock::now();
		}
		else if (tmp_count == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				// просто ждем
				if (std::chrono::steady_clock::now() - last_send_time > std::chrono::seconds(5)) {
					// словили timeout
					std::string tmp_str{ inet_ntoa(connect_ptr->connection_addr.sin_addr) };
					if (connect_ptr->account_ptr != nullptr)
						tmp_str += '(' + connect_ptr->account_ptr->last_name + ' ' + connect_ptr->account_ptr->first_name[0] + '.' + connect_ptr->account_ptr->surname[0] + ')';
					logs.insert(EL_ERROR, EL_NETWORK, "Timeout отправки данных, возможно пользователь " + tmp_str + " завис");

					return false;	// отправка не завершилась
				}
				Sleep(5);
			}
			else {
				// ошибка
				std::string tmp_str{ inet_ntoa(connect_ptr->connection_addr.sin_addr) };
				if (connect_ptr->account_ptr != nullptr)
					tmp_str += '(' + connect_ptr->account_ptr->last_name + ' ' + connect_ptr->account_ptr->first_name[0] + '.' + connect_ptr->account_ptr->surname[0] + ')';
				logs.insert(EL_ERROR, EL_NETWORK, "Ошибка отправки данных, отправка остановлена " + tmp_str);
				
				return false;	// отправка не завершилась
			}
		}
		else {	// видимо == 0
			// клиент закрыл соединение
			std::string tmp_str{ inet_ntoa(connect_ptr->connection_addr.sin_addr) };
			if (connect_ptr->account_ptr != nullptr)
				tmp_str += '(' + connect_ptr->account_ptr->last_name + ' ' + connect_ptr->account_ptr->first_name[0] + '.' + connect_ptr->account_ptr->surname[0] + ')';
			logs.insert(EL_ERROR, EL_NETWORK, "Клиент " + tmp_str + " закрыл соединение во время отправки данных");

			return false;	// отправка не завершилась
		}
	}

	return true;	// отправка успешно завершилась
}

//---------------------------------------------------------- методы классов

//---------------------- MsgHead
MsgHead::MsgHead() {
	first_code = UCHAR_MAX;
	second_code = UCHAR_MAX;
	third_code = UINT32_MAX;
	msg_length = 0;
}

int MsgHead::size_of() {
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

//---------------------- ServerData
ServerData::ServerData() {
	state_of_server = 1;	// запуск
}

ServerData::~ServerData() {
	__clear_accounts__();

}

int ServerData::get_state() {
	std::lock_guard<std::mutex> lock(state_mutex);	// залочили 
	int tmp = state_of_server;
	return tmp;
}

void ServerData::set_state(int new_state) {
	if (new_state > 1 || new_state < -1)
		new_state = -1;

	std::lock_guard<std::mutex> lock(state_mutex);
	state_of_server = new_state;
}

int ServerData::get_count_of_connections() {
	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	uint32_t tmp = connected_vect.size();
	return tmp;
}

serv_connection* ServerData::add_new_connection(const SOCKET& socket, const sockaddr_in& socketaddr) {
	serv_connection* tmp_ptr = new serv_connection;
	tmp_ptr->connection = socket;
	tmp_ptr->connection_addr = socketaddr;

	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	connected_vect.push_back(tmp_ptr);

	return tmp_ptr;
}

bool ServerData::del_connection(const SOCKET& socket) {
	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	for (uint32_t i{ 0 }; i < connected_vect.size(); i++) {
		if (connected_vect[i]->connection == socket) {
			delete connected_vect[i];	// очистил динамическую память
			connected_vect.erase(connected_vect.begin() + i);	// удалил указатель

			return true;
		}
	}

	return false;
}

bool ServerData::ReadFromFile() {
	bool tmp_bool = true;

	if (__read_from_file_accounts__() == false)
		tmp_bool = false;

	return tmp_bool;
}

void ServerData::SaveToFile() {
	__save_to_file_accounts__();

}

bool ServerData::read_from_file_accounts() {
	return __read_from_file_accounts__();
}

void ServerData::save_to_file_accounts() {
	return __save_to_file_accounts__();
}

void ServerData::sort_accounts() {
	return __sort_accounts__();
}

bool ServerData::insert_new_account(uint32_t id, uint32_t role, std::string password, std::string first_name, std::string last_name, std::string surname, std::string faculty) {
	account_note* tmp_ptr = new account_note;
	tmp_ptr->id = id;
	tmp_ptr->role = role;
	tmp_ptr->password = password;
	tmp_ptr->first_name = first_name;
	tmp_ptr->last_name = last_name;
	tmp_ptr->surname = surname;
	tmp_ptr->faculty = faculty;
	tmp_ptr->last_action = 0;
	
	std::lock_guard<std::mutex> lock(accounts_mutex);

	auto it = std::lower_bound(accounts.begin(), accounts.end(), id,
		[](const account_note* first, const uint32_t& nedded_id) {
			return first->id < nedded_id;
		});

	if (it == accounts.end() || (*it)->id != id) {	// если такого id нет (мы можем быть в конце или на месте большего (для сохранения сортировки))
		// добавлем на место it
		accounts.insert(it, tmp_ptr);

		return true;
	}
	else {
		delete tmp_ptr;

		return false;
	}
}

bool ServerData::change_account_data(const uint32_t& nedded_id, uint32_t role, std::string first_name, std::string last_name, std::string surname, std::string faculty) {
	std::vector<account_note*> tmp_accounts;
	{
		std::lock_guard<std::mutex> lock(accounts_mutex);
		tmp_accounts = accounts;
	}

	auto it = std::lower_bound(tmp_accounts.begin(), tmp_accounts.end(), nedded_id,
		[](const account_note* first, const uint32_t& nedded) {
			return first->id < nedded;
		});

	if (it == tmp_accounts.end() || (*it)->id != nedded_id)
		return false;

	{
		std::lock_guard<std::mutex> lock(accounts_mutex);
		
		(*it)->role = role;
		(*it)->first_name = first_name;
		(*it)->last_name = last_name;
		(*it)->surname = surname;
		(*it)->faculty = faculty;
	}

	return true;
}

std::vector<account_note> ServerData::get_all_account_notes() {
	std::vector<account_note> tmp_vect;
	std::vector<account_note*> tmp_ptr;

	{
		std::lock_guard<std::mutex> lock(accounts_mutex);
		
		tmp_ptr = accounts;
	}

	tmp_vect.resize(tmp_ptr.size());

	for (uint32_t i{ 0 }; i < tmp_ptr.size(); i++) {
		tmp_vect[i].id = tmp_ptr[i]->id;
		tmp_vect[i].role = tmp_ptr[i]->role;
		tmp_vect[i].password = tmp_ptr[i]->password;
		tmp_vect[i].last_action = tmp_ptr[i]->last_action;
		tmp_vect[i].first_name = tmp_ptr[i]->first_name;
		tmp_vect[i].last_name = tmp_ptr[i]->last_name;
		tmp_vect[i].surname = tmp_ptr[i]->surname;
		tmp_vect[i].faculty = tmp_ptr[i]->faculty;
	}

	return tmp_vect;
}

void ServerData::__clear_accounts__() {
	std::lock_guard<std::mutex> lock(accounts_mutex);

	for (uint32_t i{ 0 }; i < accounts.size(); i++)
		if (accounts[i] != nullptr)
			delete accounts[i];

	accounts.clear();
}

bool ServerData::__read_from_file_accounts__() {
	std::ifstream file("accounts.save", std::ios::binary | std::ios::in);

	if (file.is_open() == false)
		return false;

	// временные переменные
	std::vector<account_note*> tmp_accounts;

	uint32_t uint32_t_buffer{ 0 };
	time_t time_t_buffer{ 0 };

	try {
		file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
		tmp_accounts.resize(uint32_t_buffer);

		for (uint32_t i{ 0 }; i < tmp_accounts.size(); i++) {
			tmp_accounts[i] = new account_note;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->id = uint32_t_buffer;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->role = uint32_t_buffer;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->password.resize(uint32_t_buffer);

			file.read(&tmp_accounts[i]->password[0], tmp_accounts[i]->password.size());

			file.read(reinterpret_cast<char*>(&time_t_buffer), sizeof(time_t));
			tmp_accounts[i]->last_action = time_t_buffer;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->first_name.resize(uint32_t_buffer);

			file.read(&tmp_accounts[i]->first_name[0], tmp_accounts[i]->first_name.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->last_name.resize(uint32_t_buffer);

			file.read(&tmp_accounts[i]->last_name[0], tmp_accounts[i]->last_name.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->surname.resize(uint32_t_buffer);

			file.read(&tmp_accounts[i]->surname[0], tmp_accounts[i]->surname.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_accounts[i]->faculty.resize(uint32_t_buffer);

			file.read(&tmp_accounts[i]->faculty[0], tmp_accounts[i]->faculty.size());
		}
	}
	catch (...) {
		// словили ошибку
		for (uint32_t i{ 0 }; i < tmp_accounts.size(); i++)
			if (tmp_accounts[i] != nullptr)
				delete tmp_accounts[i];

		tmp_accounts.clear();

		file.close();

		return false;
	}

	// дошли сюда => все хорошо
	file.close();

	__clear_accounts__();

	{
		std::lock_guard<std::mutex> lock(accounts_mutex);
		accounts = tmp_accounts;
	}

	return true;
}

void ServerData::__save_to_file_accounts__() {
	std::ofstream file("accounts.save", std::ios::binary | std::ios::trunc);

	if (file.is_open() == false)
		return;

	// временная переменные
	std::vector<account_note*> tmp_accounts;

	uint32_t uint32_t_buffer;
	time_t time_t_buffer;

	{	// пренос для записи данных
		std::lock_guard<std::mutex> lock(accounts_mutex);
		tmp_accounts = accounts;
	}

	// запись в файл
	uint32_t_buffer = tmp_accounts.size();
	file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

	for (uint32_t i{ 0 }; i < tmp_accounts.size(); i++) {
		uint32_t_buffer = tmp_accounts[i]->id;
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		uint32_t_buffer = tmp_accounts[i]->role;
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		uint32_t_buffer = tmp_accounts[i]->password.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_accounts[i]->password[0], tmp_accounts[i]->password.length());

		time_t_buffer = tmp_accounts[i]->last_action;
		file.write(reinterpret_cast<char*>(&time_t_buffer), sizeof(time_t));

		uint32_t_buffer = tmp_accounts[i]->first_name.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_accounts[i]->first_name[0], tmp_accounts[i]->first_name.length());

		uint32_t_buffer = tmp_accounts[i]->last_name.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_accounts[i]->last_name[0], tmp_accounts[i]->last_name.length());

		uint32_t_buffer = tmp_accounts[i]->surname.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_accounts[i]->surname[0], tmp_accounts[i]->surname.length());

		uint32_t_buffer = tmp_accounts[i]->faculty.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_accounts[i]->faculty[0], tmp_accounts[i]->faculty.length());
	}

	file.close();
}

void ServerData::__sort_accounts__() {
	std::lock_guard<std::mutex> lock(accounts_mutex);

	std::sort(accounts.begin(), accounts.end(),
		[](const account_note* first, const account_note* second) {
			return first->id < second->id;
		});
}