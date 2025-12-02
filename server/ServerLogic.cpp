// первый char код
#define FROM_SERVER 100	
#define FROM_CLIENT 55
// второй char код
#define ACCESS_DENIED 66
#define AUTHORISATION 2
#define CREATE_NEW_TASK 22
#define GET_ALL_TASKS 33
#define GET_TASK_INFO 101
#define DELETE_TASK 44
#define GET_IO_FILE 102
#define CHANGE_TASK 103
#define CHANGE_PASSWORD 5
#define GET_ALL_SOLUTIONS 77

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
			if (connection_ptr->account_ptr != nullptr)
				connection_ptr->account_ptr->last_action = connection_ptr->last_action;

			// пробуем прочесть все возможные сообщения
			while (true) {
				if (recv_buffer.size() < msg_header.size_of())
					break;	// байтов не хватает даже на заголовок

				if (msg_header.read_from_char(&recv_buffer[0]) == false) {	// ошибка header
					std::string ip_str = inet_ntoa(connection_ptr->connection_addr.sin_addr);
					std::string tmp_str{ "Ошибка заголовка сообщения от" + ip_str};
					if (connection_ptr->account_ptr != nullptr)
						tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
					tmp_str += ". Закрываю соединение. Количество потоков: " + std::to_string(server.get_count_of_connections() - 1);
					logs.insert(EL_ERROR, EL_NETWORK, tmp_str);

					std::vector<char> tmp_data;
					CreateAccessDeniedMessage(tmp_data, "Неверный протокол общения");
					SendTo(connection_ptr, tmp_data, logs);
					Sleep(200);		// на всякий случай, чтобы успел отправить

					closesocket(connection_ptr->connection);
					server.del_connection(connection_ptr->connection);
					return;
				}

				if (recv_buffer.size() < msg_header.size_of() + msg_header.msg_length)
					break;	// байт недостаточно для сообщения
					
				if (ProcessMessage(msg_header, recv_buffer, connection_ptr, server, logs) == false) { // необходимо закрыть соединение
					// ошибку логирует Process

					Sleep(200);	// на всякий (чтобы все сообщения успели дойтиы)

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
			Sleep(10);
		}
		else { // ошибка SOCKET_ERROR
			std::string ip_str = inet_ntoa(connection_ptr->connection_addr.sin_addr);

			int error_code = WSAGetLastError();
			if (error_code == WSAECONNRESET || error_code == WSAENETRESET || error_code == WSAETIMEDOUT || error_code == WSAECONNABORTED) {
				std::string tmp_str{ "Соединение разорванно " + ip_str};
				if (connection_ptr->account_ptr != nullptr)
					tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';

				logs.insert(EL_NETWORK, tmp_str);
			}
			else {
				std::string	tmp_str{ "Ошибка приема данных от пользователя " + ip_str};
				if (connection_ptr->account_ptr != nullptr)
					tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
				tmp_str += ", закрываю соединение";
				logs.insert(EL_ERROR, EL_NETWORK, tmp_str);

				std::vector<char> tmp_data;
				CreateAccessDeniedMessage(tmp_data, "Ошибка сети");
				SendTo(connection_ptr, tmp_data, logs);

				Sleep(200);
			}

			break; // завершаем соединение
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
	std::string tmp_str{ "Соединение с " };
	tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
	if (connection_ptr->account_ptr != nullptr)
		tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
	tmp_str += " закрыто. Количество потоков: " + std::to_string(server.get_count_of_connections() - 1);

	logs.insert(EL_NETWORK, tmp_str);

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

void CreateAccessDeniedMessage(std::vector<char>& vect, std::string text) {
	unsigned char uchar_buffer{ 0 };
	uint32_t uint32_t_buffer{ 0 };
	char* tmp_ptr;

	vect.clear();
	
	uchar_buffer = FROM_SERVER;	// от сервера
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = ACCESS_DENIED;	// отказ доступа
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 0;	// пустой под_код
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = text.length() + sizeof(uint32_t);	// размер полезной инфы
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// сама полезная чать 

	uint32_t_buffer = text.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	if (text.length() > 0) // сама полезная инфа
		vect.insert(vect.end(), &text[0], &text[0] + text.length());

	return;
}

void CreateAuthorisationMessage(std::vector<char>& vect, serv_connection* connection_ptr, const uint32_t role_id) {
	std::string tmp_str;
	if (connection_ptr->account_ptr != nullptr)
		tmp_str = connection_ptr->account_ptr->first_name;
	else
		tmp_str = "none";

	// полезные данные
	std::vector<char> main_data;

	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// сам перевод
	uint32_t_buffer = tmp_str.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), &tmp_str[0], &tmp_str[0] + tmp_str.size());

	// дальше служебные
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = AUTHORISATION;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = role_id;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateConfirmCreateTaskMessage(std::vector<char>& vect, serv_connection* connection_ptr) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// начало записи
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CREATE_NEW_TASK;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

	uint32_t_buffer = 0;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));
}

void CreateGetAllTasksForTeacherMessage(std::vector<char>& vect, ServerData& server) {
	// временные переменные
	std::vector<task_note> tmp_all_tasks;

	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		tmp_all_tasks.resize(server.all_tasks.size());

		for (uint32_t i{ 0 }; i < tmp_all_tasks.size(); i++)
			tmp_all_tasks[i] = *server.all_tasks[i];
	}	// перенесли в буфер

	std::vector<char> main_data;

	uint32_t_buffer = tmp_all_tasks.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	for (uint32_t i{ 0 }; i < tmp_all_tasks.size(); i++) {
		uint32_t_buffer = tmp_all_tasks[i].task_name.length();
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

		main_data.insert(main_data.end(), &tmp_all_tasks[i].task_name[0], &tmp_all_tasks[i].task_name[0] + tmp_all_tasks[i].task_name.size());
	}

	// закончили с main_data
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_ALL_TASKS;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateGetAllTasksForUserMessage(std::vector<char>& vect, serv_connection* connection_ptr, ServerData& server) {
	// временные переменные
	std::vector<char> main_data;
	
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// main инфа
	std::vector<task_note*> tmp_tasks;
	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		tmp_tasks = server.all_tasks;
	}

	uint32_t_buffer = tmp_tasks.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	for (uint32_t i{ 0 }; i < tmp_tasks.size(); i++) {
		uint32_t_buffer = tmp_tasks[i]->task_name.length();
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

		main_data.insert(main_data.end(), tmp_tasks[i]->task_name.begin(), tmp_tasks[i]->task_name.end());

		bool is_done{ false };
		for (uint32_t h{ 0 }; h < tmp_tasks[i]->checked_accounts.size(); h++) {
			if (tmp_tasks[i]->checked_accounts[h]->account_id == connection_ptr->account_ptr->id) {
				if (tmp_tasks[i]->checked_accounts[h]->all_tryes.empty() == false)
					is_done = tmp_tasks[i]->checked_accounts[h]->all_tryes[0]->is_good;	// помечаем как первое (лучшее)

				break;	// досрочно выходим (нашли аккаунт)
			}
		}

		main_data.insert(main_data.end(), &is_done, &is_done + sizeof(bool));
	}

	// все сообщение
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_ALL_TASKS;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 1;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateGetTaskInfoForTeacherMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server) {
	// временные переменные 
	std::vector<char> main_data;

	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	task_note tmp_task;
	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		tmp_task = *server.all_tasks[butt_index];
	}

	uint32_t_buffer = tmp_task.task_name.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_task.task_name.begin(), tmp_task.task_name.end());

	uint32_t_buffer = tmp_task.task_info.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_task.task_info.begin(), tmp_task.task_info.end());

	uint32_t_buffer = tmp_task.checked_accounts.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = tmp_task.time_limit_ms;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = tmp_task.memory_limit_kb;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// дальше составление всего сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_TASK_INFO;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateGetInputFileMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// составление main_data
	std::vector<char> main_data;

	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		uint32_t_buffer = server.all_tasks[butt_index]->input_file.size();
	}
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		main_data.insert(main_data.end(), server.all_tasks[butt_index]->input_file.begin(), server.all_tasks[butt_index]->input_file.end());
	}

	// заполнение всего сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_IO_FILE;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 3;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateGetOutputFileMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// составление main_data
	std::vector<char> main_data;

	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		uint32_t_buffer = server.all_tasks[butt_index]->output_file.size();
	}
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		main_data.insert(main_data.end(), server.all_tasks[butt_index]->output_file.begin(), server.all_tasks[butt_index]->output_file.end());
	}

	// заполнение всего сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_IO_FILE;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 4;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateChangeTaskMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server) {
	// временные перемернные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// составление main_data
	std::vector<char> main_data;

	std::string tmp_name;
	std::string tmp_info;
	std::string tmp_input_file;
	std::string tmp_output_file;
	uint32_t time_limit_ms;
	uint32_t memory_limit_ms;
	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		
		tmp_name = server.all_tasks[butt_index]->task_name;
		tmp_info = server.all_tasks[butt_index]->task_info;
		tmp_input_file = server.all_tasks[butt_index]->input_file;
		tmp_output_file = server.all_tasks[butt_index]->output_file;
		time_limit_ms = server.all_tasks[butt_index]->time_limit_ms;
		memory_limit_ms = server.all_tasks[butt_index]->memory_limit_kb;
	}

	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = tmp_name.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_name.begin(), tmp_name.end());

	uint32_t_buffer = tmp_info.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_info.begin(), tmp_info.end());

	uint32_t_buffer = tmp_input_file.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_input_file.begin(), tmp_input_file.end());

	uint32_t_buffer = tmp_output_file.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), tmp_output_file.begin(), tmp_output_file.end());

	tmp_ptr = reinterpret_cast<char*>(&time_limit_ms);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	tmp_ptr = reinterpret_cast<char*>(&memory_limit_ms);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// составление основного сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_TASK;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateSuccessChangePasswordMessage(std::vector<char>& vect) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// составление готового сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_PASSWORD;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 1;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	//uint32_t_buffer = main_data.size();
	uint32_t_buffer = 0;	// т.к нет main_data NULL
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	//vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateFailChangePasswordMessage(std::vector<char>& vect) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// составление готового сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_PASSWORD;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	//uint32_t_buffer = main_data.size();
	uint32_t_buffer = 0;	// т.к нет main_data NULL
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	//vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateGetAllSolutionsMessage(std::vector<char>& vect, const std::vector<cheacks*>& sorted_solutions, const uint32_t& task_id, const id_cheack* account_tryes_ptr, const uint32_t& sort_type, ServerData& server) {
	// временные переменные
	uint32_t uint32_t_buffer;
	char* tmp_ptr;
	unsigned char uchar_buffer;
	time_t time_t_buffer;
	bool bool_buffer;

	std::vector<char> main_data;

	// main_data
	uint32_t_buffer = task_id;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = server.all_tasks[task_id]->task_name.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), server.all_tasks[task_id]->task_name.begin(), server.all_tasks[task_id]->task_name.end());

	uint32_t_buffer = server.all_tasks[task_id]->task_info.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), server.all_tasks[task_id]->task_info.begin(), server.all_tasks[task_id]->task_info.end());

	uint32_t_buffer = server.all_tasks[task_id]->time_limit_ms;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = server.all_tasks[task_id]->memory_limit_kb;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	if (account_tryes_ptr->all_tryes.empty())
		bool_buffer = false;	// значит еще ничего не присылалось
	else
		bool_buffer = account_tryes_ptr->all_tryes[0]->is_good;	// прираваем знач. лучшего ответа
	tmp_ptr = reinterpret_cast<char*>(&bool_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(bool));
	
	if (bool_buffer) {	// заполняем лучшее решение только в том случае, если у нас задание выполнено
		time_t_buffer = account_tryes_ptr->all_tryes[0]->send_time;
		tmp_ptr = reinterpret_cast<char*>(&time_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(time_t));

		uint32_t_buffer = account_tryes_ptr->all_tryes[0]->cpu_time_ms;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

		uint32_t_buffer = account_tryes_ptr->all_tryes[0]->memory_bytes;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));
	}

	uint32_t_buffer = sorted_solutions.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	for (uint32_t i{ 0 }; i < sorted_solutions.size(); i++) {
		time_t_buffer = sorted_solutions[i]->send_time;
		tmp_ptr = reinterpret_cast<char*>(&time_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(time_t));

		bool_buffer = sorted_solutions[i]->is_good;
		tmp_ptr = reinterpret_cast<char*>(&bool_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(bool));

		uint32_t_buffer = sorted_solutions[i]->cpu_time_ms;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

		uint32_t_buffer = sorted_solutions[i]->memory_bytes;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));
	}

	// заполнение всего сообщения
	vect.clear();

	uchar_buffer = FROM_SERVER;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_ALL_SOLUTIONS;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = sort_type;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (msg_header.first_code != FROM_CLIENT) {
		std::string tmp_str{ "Ошибка первичного кода сообщения от " };
		tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
		if (connection_ptr->account_ptr != nullptr)
			tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
		tmp_str += ". Закрываю соединение";
		logs.insert(EL_ERROR, EL_NETWORK, tmp_str);
		return false;	// видимо пришло не от клиента (почему-то)
	}

	switch (msg_header.second_code)
	{
	case ACCESS_DENIED:

		break;
	case AUTHORISATION:
		return ProcessAuthorisationMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case CREATE_NEW_TASK:
		return ProcessCreateNewTaskMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case GET_ALL_TASKS:
		return ProcessGetAllTasksMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case GET_TASK_INFO:
		return ProcessGetTaskInfoMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case DELETE_TASK:
		return ProcessDeleteTaskMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case GET_IO_FILE:
		return ProcessGetInputOutputFileMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case CHANGE_TASK:
		return ProcessChangeTaskMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case CHANGE_PASSWORD:
		return ProcessChangePasswordMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case GET_ALL_SOLUTIONS:
		return ProcessGetAllSolutionsMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	default:	// заглушка для неизвестных
		std::string tmp_str{ "Неизвестный вторичный код сообщения от " };
		tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
		if (connection_ptr->account_ptr != nullptr)
			tmp_str += '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')';
		tmp_str += ". Закрываю соединение";
		logs.insert(EL_ERROR, EL_NETWORK, tmp_str);
		return false;	// какой-то неизвестный код
		break;
	}
	
	return true;
}

bool ProcessAuthorisationMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	std::string tmp_login, tmp_password;
	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_login.insert(tmp_login.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_password.insert(tmp_password.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
	}
	catch (...) {
		std::string tmp_str{ "Ошибка открытия запроса от " };
		tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
		logs.insert(EL_ACTION, EL_NETWORK, EL_ERROR, tmp_str);
		return false;
	}

	// дальше проверяем 
	std::vector<account_note> accounts = server.get_all_account_notes();

	uint32_t tmp_login_uint = std::stoi(tmp_login);

	auto it = std::lower_bound(accounts.begin(), accounts.end(), tmp_login_uint,
		[](const account_note& first_ptr, const uint32_t& nedded) {
			return first_ptr.id < nedded;
		});

	if (it == accounts.end() || (*it).id != tmp_login_uint) {
		return false;
	}

	// значит нашли
	if ((*it).password != tmp_password) {
		std::string tmp_str{"Неверный пароль для пользователя (" + std::to_string((*it).id) + ')'};
		tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
		logs.insert(EL_ACTION, EL_SECURITY, EL_ERROR, tmp_str);

		return false;
	}

	// значит авторизация удачная
	connection_ptr->account_ptr = server.get_account_ptr(tmp_login_uint);
	if (connection_ptr->account_ptr != nullptr)
		connection_ptr->account_ptr->last_action = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	std::vector<char> data;
	switch ((*it).role) {
	case USER_ROLE:
		CreateAuthorisationMessage(data, connection_ptr, USER_ROLE);
		break;	
	case TEACHER_ROLE:
		CreateAuthorisationMessage(data, connection_ptr, TEACHER_ROLE);
		break;
	case ADMIN_ROLE:
		CreateAuthorisationMessage(data, connection_ptr, ADMIN_ROLE);
		break;
	default:
		return false;
		break;
	}

	bool tmp_bool = SendTo(connection_ptr, data, logs);

	logs.insert(EL_ACTION, EL_NETWORK, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + " успешно вошел, как (" + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')');

	return tmp_bool;
}

bool ProcessCreateNewTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (connection_ptr->account_ptr == nullptr || connection_ptr->account_ptr->role != TEACHER_ROLE) {
		logs.insert(EL_ERROR, EL_SECURITY, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string(connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0])) + " вышел за ограничения роли (на кол посадить его)");
		return false;
	}

	task_note tmp_task;

	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;

	uint32_t index = msg_header.size_of();

	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_task.task_name.insert(tmp_task.task_name.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
		
		tmp_task.task_info.insert(tmp_task.task_info.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_task.input_file.insert(tmp_task.input_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_task.output_file.insert(tmp_task.output_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		tmp_task.time_limit_ms = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_task.memory_limit_kb = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		// ошибка
		std::string tmp_str{ "Ошибка открытия запроса от " };
		tmp_str += inet_ntoa(connection_ptr->connection_addr.sin_addr);
		logs.insert(EL_ACTION, EL_NETWORK, EL_ERROR, tmp_str);
		return false;
	}

	server.create_new_task(tmp_task.task_name, tmp_task.task_info, tmp_task.input_file, tmp_task.output_file, tmp_task.time_limit_ms, tmp_task.memory_limit_kb);

	logs.insert(EL_ACTION, EL_JUDGE, "Преподаватель " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string(connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0])) + " успешно создал новый тест");

	std::vector<char> tmp_data;
	CreateConfirmCreateTaskMessage(tmp_data, connection_ptr);

	return SendTo(connection_ptr, tmp_data, logs);
}

bool ProcessGetAllTasksMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	// временные переменные
	std::vector<char> tmp_data;

	if (connection_ptr->account_ptr == nullptr) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + " не авторизован для команды, закрывая соединение");
		return false;
	}

	if (connection_ptr->account_ptr->role == USER_ROLE) { // для юзера
		CreateGetAllTasksForUserMessage(tmp_data, connection_ptr, server);

		logs.insert(EL_ACTION, "пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')' + " зашел в выбор заданий");

		return SendTo(connection_ptr, tmp_data, logs);
	}
	else if (connection_ptr->account_ptr->role == TEACHER_ROLE) {	// для препода
		CreateGetAllTasksForTeacherMessage(tmp_data, server);

		logs.insert(EL_ACTION, "пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')' + " зашел в выбор заданий");

		return SendTo(connection_ptr, tmp_data, logs);
	}
	else {	// значит вышли за пределы роли
		logs.insert(EL_ERROR, EL_ACTION, EL_NETWORK, "Пользователь " + (std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr))) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " вышел за пределы своей роли");
		return false;
	}
}

bool ProcessGetTaskInfoMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	// временные переменные
	std::vector<char> tmp_data;
	uint32_t butt_index;

	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		butt_index = uint32_t_buffer;
	}
	catch (...) {
		logs.insert(EL_ACTION, EL_NETWORK, EL_ERROR, "Ошибка открытия запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + " закрываю соединение");
		return false;
	}

		
	if (butt_index >= server.all_tasks.size()) {
		logs.insert(EL_ERROR, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + std::string(((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " в запросе вышел а пределы хранилища"));
	}

	// распределение
	if (connection_ptr->account_ptr == nullptr) {
		logs.insert(EL_ERROR, EL_ACTION, EL_NETWORK, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + " не авторизован, закрываю соединение");
		return false;
	}

	if (connection_ptr->account_ptr->role == USER_ROLE) {




	}
	else if (connection_ptr->account_ptr->role == TEACHER_ROLE) {

		CreateGetTaskInfoForTeacherMessage(tmp_data, butt_index, server);

		{
			std::lock_guard<std::mutex> lock(server.tasks_mutex);
			logs.insert(EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + '(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')' + " зашел на задание (" + server.all_tasks[butt_index]->task_name + ')');
		}
		return SendTo(connection_ptr, tmp_data, logs);
	}
	else {	// вышли за пределы роли
		logs.insert(EL_ERROR, EL_ACTION, EL_NETWORK, "Пользователь " + (std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr))) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " вышел за пределы своей роли");
		return false;
	}
}

bool ProcessDeleteTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (connection_ptr->account_ptr == nullptr || connection_ptr->account_ptr->role != TEACHER_ROLE) {
		logs.insert(EL_ERROR, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " вышел за пределы роли (удаление задания)");
		return false;
	}
	
	// временные переменные
	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock1(server.tasks_mutex);

		if (uint32_t_buffer > server.all_tasks.size() - 1) {
			logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка id удаления от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
			return false;
		}

		// очищаем динамическую память
		if (server.all_tasks[uint32_t_buffer] != nullptr) {
			for (uint32_t g{ 0 }; g < server.all_tasks[uint32_t_buffer]->checked_accounts.size(); g++) {
				if (server.all_tasks[uint32_t_buffer]->checked_accounts[g] != nullptr) {
					std::lock_guard<std::mutex> lock2(server.all_tasks[uint32_t_buffer]->checked_accounts[g]->account_id_cheak_mutex);

					for (uint32_t h{ 0 }; h < server.all_tasks[uint32_t_buffer]->checked_accounts[g]->all_tryes.size(); h++) {
						if (server.all_tasks[uint32_t_buffer]->checked_accounts[g]->all_tryes[h] != nullptr)
							delete server.all_tasks[uint32_t_buffer]->checked_accounts[g]->all_tryes[h];
					}

					delete server.all_tasks[uint32_t_buffer]->checked_accounts[g];
				}
			}

			delete server.all_tasks[uint32_t_buffer];
		}

		server.all_tasks.erase(server.all_tasks.begin() + uint32_t_buffer);	// удалили указатель
	}

	std::vector<char> tmp_data;
	CreateGetAllTasksForTeacherMessage(tmp_data, server);
	
	return SendTo(connection_ptr, tmp_data, logs);
}

bool ProcessGetInputOutputFileMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (connection_ptr->account_ptr == nullptr || connection_ptr->account_ptr->role != TEACHER_ROLE) {
		logs.insert(EL_ERROR, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " вышел за пределы роли (удаление задания)");
		return false;
	}

	// временные переменные
	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock1(server.tasks_mutex);
		if (uint32_t_buffer > server.all_tasks.size() - 1) {
			logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка id (io file) от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
			return false;
		}
	}

	// все хорошо, можем отправлять input файл
	std::vector<char> tmp_data;

	if (msg_header.third_code == 1)
		CreateGetInputFileMessage(tmp_data, uint32_t_buffer, server);
	else if (msg_header.third_code == 2)
		CreateGetOutputFileMessage(tmp_data, uint32_t_buffer, server);
	else {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка third_code (io file) от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	logs.insert(EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')') + " открыл " + ((msg_header.third_code == 1) ? "input" : "output") + " файл задания: " + std::to_string(uint32_t_buffer) + '(' + server.all_tasks[uint32_t_buffer]->task_name + ')');

	return SendTo(connection_ptr, tmp_data, logs);
}

bool ProcessChangeTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (connection_ptr->account_ptr == nullptr || connection_ptr->account_ptr->role != TEACHER_ROLE) {
		logs.insert(EL_ERROR, EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " вышел за пределы роли (удаление задания)");
		return false;
	}

	switch (msg_header.third_code)
	{
	case 0:	// запрос просто на менюшку
		return ProcessGetChangeTaskMenuMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	case 103:	// запрос на изменения
		return ProcessChangeThatTaskMessage(msg_header, recv_buffer, connection_ptr, server, logs);
		break;
	default:
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса third_code от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
		break;
	}
}

bool ProcessGetChangeTaskMenuMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	// временные переменные
	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock1(server.tasks_mutex);
		if (uint32_t_buffer > server.all_tasks.size() - 1) {
			logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка id (io file) от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
			return false;
		}
	}

	// все хорошо, составляем сообщение
	std::vector<char> tmp_data;

	CreateChangeTaskMessage(tmp_data, uint32_t_buffer, server);

	return SendTo(connection_ptr, tmp_data, logs);
}

bool ProcessChangeThatTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	// временные переменные
	uint32_t uint32_t_buffer;

	uint32_t butt_index, time_limit_ms, memory_limit_kb;
	std::string name, info, input_file, output_file;
	bool is_del_tryes;

	uint32_t index = msg_header.size_of();
	try {
		butt_index = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		name.insert(name.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		info.insert(info.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		input_file.insert(input_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		output_file.insert(output_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;
		
		time_limit_ms = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		memory_limit_kb = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		is_del_tryes = *reinterpret_cast<const bool*>(&recv_buffer[index]);
		index += sizeof(bool);
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	{
		std::lock_guard<std::mutex> lock1(server.tasks_mutex);
		if (butt_index > server.all_tasks.size() - 1) {
			logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка id (io file) от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
			return false;
		}
	}

	// все хорошо, можем изменять
	task_note* task_ptr = new task_note;
	task_ptr->task_name = name;
	task_ptr->task_info = info;
	task_ptr->input_file = input_file;
	task_ptr->output_file = output_file;
	task_ptr->time_limit_ms = time_limit_ms;
	task_ptr->memory_limit_kb = memory_limit_kb;

	if (is_del_tryes) {	// удаляем все решения задания
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		if (server.all_tasks[butt_index] != nullptr) {
			for (uint32_t i{ 0 }; i < server.all_tasks[butt_index]->checked_accounts.size(); i++) {
				if (server.all_tasks[butt_index]->checked_accounts[i] != nullptr) {
					for (uint32_t g{ 0 }; g < server.all_tasks[butt_index]->checked_accounts[i]->all_tryes.size(); g++) {
						if (server.all_tasks[butt_index]->checked_accounts[i]->all_tryes[g] != nullptr) {
							delete server.all_tasks[butt_index]->checked_accounts[i]->all_tryes[g];
						}
					}
					delete server.all_tasks[butt_index]->checked_accounts[i];
				}
			}
			delete server.all_tasks[butt_index];
		}

		server.all_tasks[butt_index] = task_ptr;	// перезаписали данные
	}
	else {	// оставляем все решения
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		
		if (server.all_tasks[butt_index] != nullptr) {
			task_ptr->checked_accounts = server.all_tasks[butt_index]->checked_accounts;	// скопировали указатель всех решений
			delete server.all_tasks[butt_index];	// удалили изначальные данные
		}
		server.all_tasks[butt_index] = task_ptr;	// переаписали данные
	}

	logs.insert(EL_ACTION, "Пользователь " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + std::string((connection_ptr->account_ptr == nullptr) ? "" : ('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + " Отредактировал задание");

	std::vector<char> tmp_data;

	CreateGetTaskInfoForTeacherMessage(tmp_data, butt_index, server);

	return SendTo(connection_ptr, tmp_data, logs);
}

bool ProcessChangePasswordMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	// временные переменные
	uint32_t uint32_t_buffer;

	std::string cur_pswd, new_pswd;

	// чтение main_data
	uint32_t index = msg_header.size_of();

	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		cur_pswd.insert(cur_pswd.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		new_pswd.insert(new_pswd.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка чтения запроса от " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");

		return false;
	}

	// дальше сама логика
	if (connection_ptr == nullptr || connection_ptr->account_ptr == nullptr || new_pswd.empty()) {
		logs.insert(EL_ERROR, EL_NETWORK, EL_ACTION, "Ошибка обработки запроса смены пароля " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");

		return false;
	}

	std::vector<char> data;

	if (connection_ptr->account_ptr->password == cur_pswd) {
		connection_ptr->account_ptr->password = new_pswd;	// изменили пароль

		logs.insert(EL_ACTION, EL_SECURITY, "Смена пароля пользователем: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')));

		CreateSuccessChangePasswordMessage(data);
	}
	else {
		// значит не верный изнач пароль
		logs.insert(EL_ACTION, EL_SECURITY, "Неверный изначальный пароль при попытке смены пароля от: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')));

		CreateFailChangePasswordMessage(data);
	}

	return SendTo(connection_ptr, data, logs);
}

bool ProcessGetAllSolutionsMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs) {
	if (connection_ptr == nullptr || connection_ptr->account_ptr == nullptr) {
		logs.insert(EL_ERROR, EL_ACTION, "Ошибка обработки запроса всех заданий от: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	if (connection_ptr->account_ptr->role != USER_ROLE) {
		logs.insert(EL_ERROR, EL_ACTION, "Выход за границы роли от: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	// временные переменные
	uint32_t uint32_t_buffer;

	// читаем сообщение
	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		logs.insert(EL_ERROR, EL_NETWORK, "Ошибка чтения запроса на все решения от: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}
	
	if (uint32_t_buffer > server.all_tasks.size() - 1) {
		logs.insert(EL_ERROR, EL_ACTION, "Ошибка id task в запросе на все решения: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
	}

	std::vector<cheacks*> tmp_all_cheaks;
	id_cheack* tmp_id_cheack_ptr{ nullptr };
	{
		std::lock_guard<std::mutex> lock(server.tasks_mutex);
		
		// нужно найти аккаунт
		auto it = server.all_tasks[uint32_t_buffer]->checked_accounts.begin();
		while (it != server.all_tasks[uint32_t_buffer]->checked_accounts.end()) {
			if ((*it)->account_id == connection_ptr->account_ptr->id) { // значит нашли нужный
				tmp_all_cheaks = (*it)->all_tryes;	// скопировали все проверки
				tmp_id_cheack_ptr = *it;
				break;
			}

			it++;	// увеличиваем иттератор
		}

		if (it == server.all_tasks[uint32_t_buffer]->checked_accounts.end()) {	// значит не нашли даже запись о аккаунте
			// создаем пустую запись 

			id_cheack* tmp_ptr2 = new id_cheack;

			tmp_ptr2->account_id = connection_ptr->account_ptr->id; // приравняли id

			server.all_tasks[uint32_t_buffer]->checked_accounts.push_back(tmp_ptr2);

			tmp_all_cheaks = tmp_ptr2->all_tryes;
			tmp_id_cheack_ptr = tmp_ptr2;
		}
	}

	// дальше составляем нужную сортировку
	switch (msg_header.third_code) {
	case 0:
		// костыль (чтобы не попадал в default)
		break;
	case 1:	// сортировка по памяти
		std::sort(tmp_all_cheaks.begin(), tmp_all_cheaks.end(),
			[](const cheacks* first, const cheacks* second) {
				if (first->is_good == second->is_good) {
					if (first->memory_bytes == second->memory_bytes) {
						
						return first->cpu_time_ms < second->cpu_time_ms;	// в начало ставим с меньшим временем (<)
					}

					return first->memory_bytes < second->memory_bytes;	// в начало ставим с меньшей памятью (<)
				}

				return first->is_good > second->is_good;	// в начало ставим true (>)
			});
		break;
	case 2:	// сортировка по времени сдачи (send_time)
		std::sort(tmp_all_cheaks.begin(), tmp_all_cheaks.end(),
			[](const cheacks* first, const cheacks* second) {
				if (first->is_good == second->is_good) {
					if (first->send_time == second->send_time) {
						if (first->cpu_time_ms == second->cpu_time_ms) {

							return first->memory_bytes < second->memory_bytes;	// в начало ставим с меньшей памятью
						}

						return first->cpu_time_ms < second->cpu_time_ms;	// в начало ставим быстреейшее (<)
					}

					return first->send_time < second->send_time;	// в начало более ранее (<)
				}

				return first->is_good > second->is_good;	// в начало true (>) большее
			});
		break;
	default:	// ошибка код
		logs.insert(EL_ERROR, EL_ACTION, "Ошибка необходимой сортировки в запросе по всем решениям от: " + std::string(inet_ntoa(connection_ptr->connection_addr.sin_addr)) + ((connection_ptr->account_ptr == nullptr) ? "" : std::string('(' + connection_ptr->account_ptr->last_name + ' ' + connection_ptr->account_ptr->first_name[0] + '.' + connection_ptr->account_ptr->surname[0] + ')')) + ", закрываю соединение");
		return false;
		break;
	}

	// массив отсортирован => генерируем ответ

	std::vector<char> data;

	CreateGetAllSolutionsMessage(data, tmp_all_cheaks, uint32_t_buffer, tmp_id_cheack_ptr, msg_header.third_code, server);

	return SendTo(connection_ptr, data, logs);
}

//---------------------------------------------------------- методы классов

//---------------------- MsgHead
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

//---------------------- ServerData
ServerData::ServerData() {
	state_of_server = 1;	// запуск
}

ServerData::~ServerData() {
	__clear_accounts__();
	__clear_all_tasks__();
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
	bool tmp_bool = __read_from_file_accounts__();
	tmp_bool = __read_from_file_all_tasks__() && tmp_bool;

	return tmp_bool;
}

void ServerData::SaveToFile() {
	__save_to_file_accounts__();
	__save_to_file_all_tasks__();

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

account_note* ServerData::get_account_ptr(uint32_t nedded_id) {
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
		return nullptr;

	return *it;
}

uint32_t ServerData::get_count_of_all_tasks() {
	std::lock_guard<std::mutex> lock(tasks_mutex);
	
	return all_tasks.size();
}

void ServerData::create_new_task(const std::string& name, const std::string& info, const std::string& input, const std::string& output, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb) {
	task_note* tmp_ptr = new task_note;

	tmp_ptr->task_name = name;
	tmp_ptr->task_info = info;
	tmp_ptr->input_file = input;
	tmp_ptr->output_file = output;
	tmp_ptr->time_limit_ms = time_limit_ms;
	tmp_ptr->memory_limit_kb = memory_limit_kb;

	std::lock_guard<std::mutex> lock(tasks_mutex);
	all_tasks.push_back(tmp_ptr);
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

void ServerData::__clear_all_tasks__() {
	std::lock_guard<std::mutex> lock(tasks_mutex);

	for (uint32_t i{ 0 }; i < all_tasks.size(); i++) {
		if (all_tasks[i] != nullptr) {
			for (uint32_t g{ 0 }; g < all_tasks[i]->checked_accounts.size(); g++) {
				if (all_tasks[i]->checked_accounts[g] != nullptr) {
					for (uint32_t h{ 0 }; h < all_tasks[i]->checked_accounts[g]->all_tryes.size(); h++) {
						if (all_tasks[i]->checked_accounts[g]->all_tryes[h] != nullptr) {
							delete all_tasks[i]->checked_accounts[g]->all_tryes[h];
						}
					}

					delete all_tasks[i]->checked_accounts[g];
				}
			}

			delete all_tasks[i];
		}
	}

	all_tasks.clear();
}

bool ServerData::__read_from_file_all_tasks__() {
	std::ifstream file("all_task.save", std::ios::binary | std::ios::in);

	if (file.is_open() == false)
		return false;

	// временные переменные
	std::vector<task_note*> tmp_all_tasks;

	uint32_t uint32_t_buffer{ 0 };
	bool bool_buffer{ false };

	// дальше само чтение
	try {
		file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
		tmp_all_tasks.resize(uint32_t_buffer);

		for (uint32_t i{ 0 }; i < tmp_all_tasks.size(); i++) {
			tmp_all_tasks[i] = new task_note;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_all_tasks[i]->task_name.resize(uint32_t_buffer);

			file.read(&tmp_all_tasks[i]->task_name[0], tmp_all_tasks[i]->task_name.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_all_tasks[i]->task_info.resize(uint32_t_buffer);

			file.read(&tmp_all_tasks[i]->task_info[0], tmp_all_tasks[i]->task_info.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_all_tasks[i]->input_file.resize(uint32_t_buffer);

			file.read(&tmp_all_tasks[i]->input_file[0], tmp_all_tasks[i]->input_file.size());

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_all_tasks[i]->output_file.resize(uint32_t_buffer);

			file.read(&tmp_all_tasks[i]->output_file[0], tmp_all_tasks[i]->output_file.size());

			file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->time_limit_ms), sizeof(uint32_t));

			file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->memory_limit_kb), sizeof(uint32_t));

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			tmp_all_tasks[i]->checked_accounts.resize(uint32_t_buffer);

			for (uint32_t g{ 0 }; g < tmp_all_tasks[i]->checked_accounts.size(); g++) {
				tmp_all_tasks[i]->checked_accounts[g] = new id_cheack;

				file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->checked_accounts[g]->account_id), sizeof(uint32_t));

				file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
				tmp_all_tasks[i]->checked_accounts[g]->all_tryes.resize(uint32_t_buffer);

				for (uint32_t h{ 0 }; h < tmp_all_tasks[i]->checked_accounts[g]->all_tryes.size(); h++) {
					tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h] = new cheacks;

					file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->is_good), sizeof(bool));
					
					file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->send_time), sizeof(time_t));

					file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
					tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->info.resize(uint32_t_buffer);

					file.read(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[g]->info[0], tmp_all_tasks[i]->checked_accounts[g]->all_tryes[g]->info.size());

					file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
					tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file.resize(uint32_t_buffer);

					file.read(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file[0], tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file.size());

					file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->memory_bytes), sizeof(uint32_t));
					file.read(reinterpret_cast<char*>(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpu_time_ms), sizeof(uint32_t));
				}
			}
		}
	}
	catch (...) {	// значит была какая-то ошибка
		for (uint32_t i{ 0 }; i < tmp_all_tasks.size(); i++)
			if (tmp_all_tasks[i] != nullptr)
				delete tmp_all_tasks[i];

		file.close();

		return false;
	}

	file.close();

	// значит ошибок не было, удаляем старое
	__clear_all_tasks__();

	{
		std::lock_guard<std::mutex> lock(tasks_mutex);
		all_tasks = tmp_all_tasks;
	}

	return true;
}

void ServerData::__save_to_file_all_tasks__() {
	std::ofstream file("all_task.save", std::ios::binary | std::ios::trunc);

	if (file.is_open() == false)
		return;

	// временная переменные;
	uint32_t uint32_t_buffer;
	time_t time_t_buffer;
	bool bool_buffer;

	std::vector<task_note*> tmp_all_tasks;

	{
		std::lock_guard<std::mutex> lock(tasks_mutex);
		tmp_all_tasks = all_tasks;
	}

	// сама запись
	uint32_t_buffer = tmp_all_tasks.size();
	file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

	for (uint32_t i{ 0 }; i < tmp_all_tasks.size(); i++) {
		uint32_t_buffer = tmp_all_tasks[i]->task_name.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_all_tasks[i]->task_name[0], tmp_all_tasks[i]->task_name.size());

		uint32_t_buffer = tmp_all_tasks[i]->task_info.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_all_tasks[i]->task_info[0], tmp_all_tasks[i]->task_info.size());

		uint32_t_buffer = tmp_all_tasks[i]->input_file.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_all_tasks[i]->input_file[0], tmp_all_tasks[i]->input_file.size());

		uint32_t_buffer = tmp_all_tasks[i]->output_file.length();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		file.write(&tmp_all_tasks[i]->output_file[0], tmp_all_tasks[i]->output_file.size());

		file.write(reinterpret_cast<char*>(&tmp_all_tasks[i]->time_limit_ms), sizeof(uint32_t));

		file.write(reinterpret_cast<char*>(&tmp_all_tasks[i]->memory_limit_kb), sizeof(uint32_t));

		uint32_t_buffer = tmp_all_tasks[i]->checked_accounts.size();
		file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

		for (uint32_t g{ 0 }; g < tmp_all_tasks[i]->checked_accounts.size(); g++) {
			uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->account_id;
			file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

			uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes.size();
			file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

			for (uint32_t h{ 0 }; h < tmp_all_tasks[i]->checked_accounts[g]->all_tryes.size(); h++) {
				bool_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->is_good;
				file.write(reinterpret_cast<char*>(&bool_buffer), sizeof(bool));

				time_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->send_time;
				file.write(reinterpret_cast<char*>(&time_t_buffer), sizeof(time_t));

				uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->info.length();
				file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

				file.write(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->info[0], tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->info.size());

				uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file.length();
				file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

				file.write(&tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file[0], tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpp_file.size());

				uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->memory_bytes;
				file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));

				uint32_t_buffer = tmp_all_tasks[i]->checked_accounts[g]->all_tryes[h]->cpu_time_ms;
				file.write(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t));
			}
		}
	}

	file.close();
}