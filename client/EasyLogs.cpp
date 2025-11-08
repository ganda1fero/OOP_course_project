#include "EasyLogs.h"

// public 

EasyLogs::EasyLogs() {
	logs_name_ = "";
	is_open_ = false;

	std::setlocale(LC_ALL, "");
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
}

EasyLogs::EasyLogs(std::string name) : EasyLogs() {
	open(name);
}

EasyLogs::EasyLogs(std::vector<char> data) : EasyLogs() {
	open_via_char(data);
}

EasyLogs::~EasyLogs() {
	Clear();
	txt_file_.close();
}

bool EasyLogs::open(std::string name) {
	std::string tmp_name = logs_name_;
	logs_name_ = name;

	if (ReadFromFile() == false) {
		logs_name_ = tmp_name;
		return false;
	}

	txt_file_.close();

	txt_file_.open(logs_name_ + ".elt", std::ios::out | std::ios::app);	// открыли дл€ записи (текст)

	if (txt_file_.is_open() == false) 
		return false;

	is_open_ = true;
	return true;
}

bool EasyLogs::open_via_char(std::vector<char> data) {
	if (OpenViaCharData(data) == false) {
		return false;
	}

	txt_file_.close();

	logs_name_.clear();

	is_open_ = true;
	return true;
}

bool EasyLogs::is_open() {
	return is_open_;
}

bool EasyLogs::create(std::string name) {
	if (name.empty())
		return false;

	close();

	logs_name_ = name;

	if (save() == false) {
		close();

		return false;
	}
	open(name);	// чтобы €вно открылс€ txt файл

	return true;
}

bool EasyLogs::save() {
	if (logs_name_.empty())
		return false;

	SaveToFile();

	return true;
}

bool EasyLogs::save_as(std::string name) {
	std::string parent_name = logs_name_;	// запомнили им€
	logs_name_ = name;	// временно дали им€ дл€ сохранени€

	bool tmp_flag = save();

	logs_name_ = parent_name;

	return tmp_flag;
}

void EasyLogs::close() {
	is_open_ = false;
	txt_file_.close();

	logs_name_.clear();

	Clear();
}

void EasyLogs::select_all(std::vector<char>& vector) {
	// временные переменные
	data_mutex.lock();
	std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;	// скопировали на момент запроса
	data_mutex.unlock();

	__put_main_data__(vector, AllLogs_data_, 0, AllLogs_data_.size());

	__put_empty_types__(vector);
}

void EasyLogs::select_all(unsigned char type, std::vector<char>& vector) {
	if (type > 7)
		return select_all(vector);

	std::vector<LogNote*> tmp_vector;

	data_mutex.lock();
	switch (type)
	{
		case EL_ERROR:
			tmp_vector = ErrorLogs_;
			break;
		case EL_SYSTEM:
			tmp_vector = SystemLogs_;
			break;
		case EL_SECURITY:
			tmp_vector = SecurityLogs_;
			break;
		case EL_AUTH:
			tmp_vector = AuthLogs_;
			break;
		case EL_ACTION:
			tmp_vector = ActionLogs_;
			break;
		case EL_JUDGE:
			tmp_vector = JudgeLogs_;
			break;
		case EL_NETWORK:
			tmp_vector = NetworkLogs_;
			break;
		default:
			data_mutex.unlock();
			return;
			break;
	}
	data_mutex.unlock();

	__put_main_data__(vector, tmp_vector, 0, tmp_vector.size());

	__put_empty_types__(vector);
}

void EasyLogs::select_from(time_t time_from, std::vector<char>& vector) {
	data_mutex.lock();
	std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;
	data_mutex.unlock();

	auto it = std::lower_bound(AllLogs_data_.begin(), AllLogs_data_.end(), time_from,
		[](const LogNote* note, const time_t& need_time) {
			return note->time < need_time;
		});

	uint32_t first_index = std::distance(AllLogs_data_.begin(), it);

	__put_main_data__(vector, AllLogs_data_, first_index, AllLogs_data_.size());

	__put_empty_types__(vector);
}

void EasyLogs::select_from(unsigned char type, time_t time_from, std::vector<char>& vector) {
	std::vector<LogNote*> tmp_vector;

	data_mutex.lock();
	switch (type)
	{
	case EL_ERROR:
		tmp_vector = ErrorLogs_;
		break;
	case EL_SYSTEM:
		tmp_vector = SystemLogs_;
		break;
	case EL_SECURITY:
		tmp_vector = SecurityLogs_;
		break;
	case EL_AUTH:
		tmp_vector = AuthLogs_;
		break;
	case EL_ACTION:
		tmp_vector = ActionLogs_;
		break;
	case EL_JUDGE:
		tmp_vector = JudgeLogs_;
		break;
	case EL_NETWORK:
		tmp_vector = NetworkLogs_;
		break;
	default:
		data_mutex.unlock();
		return;
		break;
	}
	data_mutex.unlock();

	auto it = std::lower_bound(tmp_vector.begin(), tmp_vector.end(), time_from,
		[](const LogNote* note, const time_t& need_time) {
			return note->time < need_time;
		});

	uint32_t first_index = std::distance(tmp_vector.begin(), it);

	__put_main_data__(vector, tmp_vector, first_index, tmp_vector.size());

	__put_empty_types__(vector);
}

void EasyLogs::select_to(time_t time_to, std::vector<char>& vector) {
	data_mutex.lock();
	std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;
	data_mutex.unlock();

	time_to += 1;

	auto it = std::lower_bound(AllLogs_data_.begin(), AllLogs_data_.end(), time_to,
		[](const LogNote* log, const time_t& time) {
			return log->time < time;
		});

	uint32_t last_index = std::distance(AllLogs_data_.begin(), it);

	__put_main_data__(vector, AllLogs_data_, 0, last_index);

	__put_empty_types__(vector);
}

void EasyLogs::select_to(unsigned char type, time_t time_to, std::vector<char>& vector) {
	std::vector<LogNote*> tmp_vector;

	data_mutex.lock();
	switch (type)
	{
	case EL_ERROR:
		tmp_vector = ErrorLogs_;
		break;
	case EL_SYSTEM:
		tmp_vector = SystemLogs_;
		break;
	case EL_SECURITY:
		tmp_vector = SecurityLogs_;
		break;
	case EL_AUTH:
		tmp_vector = AuthLogs_;
		break;
	case EL_ACTION:
		tmp_vector = ActionLogs_;
		break;
	case EL_JUDGE:
		tmp_vector = JudgeLogs_;
		break;
	case EL_NETWORK:
		tmp_vector = NetworkLogs_;
		break;
	default:
		data_mutex.unlock();
		return;
		break;
	}
	data_mutex.unlock();

	time_to += 1;

	auto it = std::lower_bound(tmp_vector.begin(), tmp_vector.end(), time_to,
		[](const LogNote* log, const time_t& time) {
			return log->time < time;
		});

	uint32_t last_index = std::distance(tmp_vector.begin(), it);

	__put_main_data__(vector, tmp_vector, 0, last_index);

	__put_empty_types__(vector);
}

void EasyLogs::select_from_to(time_t time_from, time_t time_to, std::vector<char>& vector) {
	data_mutex.lock();
	std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;
	data_mutex.unlock();
	
	// начальный индекс
	auto it = std::lower_bound(AllLogs_data_.begin(), AllLogs_data_.end(), time_from,
		[](const LogNote* note, const time_t& need_time) {
			return note->time < need_time;
		});

	uint32_t first_index = std::distance(AllLogs_data_.begin(), it);

	// конечный индекс
	time_to += 1;

	it = std::lower_bound(AllLogs_data_.begin(), AllLogs_data_.end(), time_to,
		[](const LogNote* log, const time_t& time) {
			return log->time < time;
		});

	uint32_t last_index = std::distance(AllLogs_data_.begin(), it);
	
	__put_main_data__(vector, AllLogs_data_, first_index, last_index);

	__put_empty_types__(vector);
}

void EasyLogs::select_from_to(unsigned char type, time_t time_from, time_t time_to, std::vector<char>& vector) {
	std::vector<LogNote*> tmp_vector;

	data_mutex.lock();
	switch (type)
	{
	case EL_ERROR:
		tmp_vector = ErrorLogs_;
		break;
	case EL_SYSTEM:
		tmp_vector = SystemLogs_;
		break;
	case EL_SECURITY:
		tmp_vector = SecurityLogs_;
		break;
	case EL_AUTH:
		tmp_vector = AuthLogs_;
		break;
	case EL_ACTION:
		tmp_vector = ActionLogs_;
		break;
	case EL_JUDGE:
		tmp_vector = JudgeLogs_;
		break;
	case EL_NETWORK:
		tmp_vector = NetworkLogs_;
		break;
	default:
		data_mutex.unlock();
		return;
		break;
	}
	data_mutex.unlock();

	// начальный индекс
	auto it = std::lower_bound(tmp_vector.begin(), tmp_vector.end(), time_from,
		[](const LogNote* note, const time_t& need_time) {
			return note->time < need_time;
		});

	uint32_t first_index = std::distance(tmp_vector.begin(), it);

	// конечный индекс
	time_to += 1;

	it = std::lower_bound(tmp_vector.begin(), tmp_vector.end(), time_to,
		[](const LogNote* log, const time_t& time) {
			return log->time < time;
		});

	uint32_t last_index = std::distance(tmp_vector.begin(), it);

	__put_main_data__(vector, tmp_vector, first_index, last_index);

	__put_empty_types__(vector);
}

bool EasyLogs::insert(const unsigned char& type, const std::string& text) {
	return AddLogBack({ type }, text);
}

bool EasyLogs::insert(const unsigned char& type1, const unsigned char& type2, const std::string& text) {
	return AddLogBack({ type1, type2 }, text);
}

bool EasyLogs::insert(const unsigned char& type1, const unsigned char& type2, const unsigned char& type3, const std::string& text) {
	return AddLogBack({ type1, type2, type3 }, text);
}

// private

bool EasyLogs::ReadFromFile() {
	if (logs_name_.empty())
		return false;

	std::ifstream file(logs_name_ + ".elb", std::ios::binary | std::ios::in);
	if (file.is_open() == false)
		return false;

	// временные переменные
	uint32_t uint32_t_buffer;
	time_t time_t_buffer;
	unsigned char uchar_buffer;

	std::vector<LogNote*> AllLogs_data_;

	std::vector<LogNote*> ErrorLogs_;
	std::vector<LogNote*> SystemLogs_;
	std::vector<LogNote*> SecurityLogs_;
	std::vector<LogNote*> AuthLogs_;
	std::vector<LogNote*> ActionLogs_;
	std::vector<LogNote*> JudgeLogs_;
	std::vector<LogNote*> NetworkLogs_;

	// само чтение с проверкой
	try {
		file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t_buffer));
		AllLogs_data_.reserve(uint32_t_buffer * 2);	// зарезервировали X2 пам€ти
		AllLogs_data_.resize(uint32_t_buffer);

		for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++) {	// чтение главных данных
			AllLogs_data_[i] = new LogNote;	// создали под указателем

			AllLogs_data_[i]->parent_index = i;	// восстановили логически

			file.read(reinterpret_cast<char*>(&time_t_buffer), sizeof(time_t_buffer));
			AllLogs_data_[i]->time = time_t_buffer;

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t_buffer));
			AllLogs_data_[i]->log_types.resize(uint32_t_buffer);

			for (uint32_t g{ 0 }; g < AllLogs_data_[i]->log_types.size(); g++) {
				file.read(reinterpret_cast<char*>(&uchar_buffer), sizeof(uchar_buffer));
				AllLogs_data_[i]->log_types[g] = uchar_buffer;
			}

			file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t_buffer));
			AllLogs_data_[i]->log_text.resize(uint32_t_buffer);
			
			if (uint32_t_buffer > 0)
				file.read(&AllLogs_data_[i]->log_text[0], uint32_t_buffer);
		}

		// ƒл€ ErrorLogs_
		__read_other_logs__(file, AllLogs_data_, ErrorLogs_);

		// ƒл€ SystemLogs_
		__read_other_logs__(file, AllLogs_data_, SystemLogs_);

		// ƒл€ SecurityLogs_
		__read_other_logs__(file, AllLogs_data_, SecurityLogs_);

		// ƒл€ AuthLogs_
		__read_other_logs__(file, AllLogs_data_, AuthLogs_);

		// ƒл€ ActionLogs_
		__read_other_logs__(file, AllLogs_data_, ActionLogs_);

		// ƒл€ JudgeLogs_
		__read_other_logs__(file, AllLogs_data_, JudgeLogs_);

		// ƒл€ NetworkLogs_
		__read_other_logs__(file, AllLogs_data_, NetworkLogs_);

		// конец чтени€ файла
		if (file.good() == false && file.eof() == false) {
			file.close();

			for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++)
				if (AllLogs_data_[i] != nullptr)
					delete AllLogs_data_[i];	// очищаем то, что успело записатьс€

			return false;
		}
	}
	catch (...) {
		file.close();

		for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++)
			if (AllLogs_data_[i] != nullptr)
				delete AllLogs_data_[i];	// очищаем то, что успело записатьс€

		return false;	// закрываем не изменив изначальные данные
	}

	file.close();

	Clear();	// очистим изначальные данные

	// переносим данные
	data_mutex.lock();
	this->AllLogs_data_ = AllLogs_data_;

	this->ErrorLogs_ = ErrorLogs_;
	this->SystemLogs_ = SystemLogs_;
	this->SecurityLogs_ = SecurityLogs_;
	this->AuthLogs_ = AuthLogs_;
	this->ActionLogs_ = ActionLogs_;
	this->JudgeLogs_ = JudgeLogs_;
	this->NetworkLogs_ = NetworkLogs_;
	data_mutex.unlock();

	return true;
}

void EasyLogs::__read_other_logs__(std::ifstream& file, const std::vector<LogNote*>& main_vector, std::vector<LogNote*>& other_vector) {
	uint32_t uint32_t_buffer;

	file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t_buffer));
	other_vector.reserve(uint32_t_buffer * 2);
	other_vector.resize(uint32_t_buffer);

	for (uint32_t i{ 0 }; i < other_vector.size(); i++) {
		file.read(reinterpret_cast<char*>(&uint32_t_buffer), sizeof(uint32_t_buffer));
		other_vector[i] = main_vector[uint32_t_buffer];
	}
}

bool EasyLogs::SaveToFile() {
	if (logs_name_.empty())
		return false;

	std::vector<char> save_data;

	if (GetCharAllData(save_data) == false) {
		// не удалось сохранить
		return false;
	}
	// сохраненеие удачно, записываем

	std::ofstream file(logs_name_ + ".elb", std::ios::binary | std::ios::out | std::ios::trunc);
	if (file.is_open() == false)
		return false;

	file.write(&save_data[0], save_data.size());

	file.close();

	return true;
}

bool EasyLogs::GetCharAllData(std::vector<char>& vector) {	// сохран€ем все
	// копируем данные (указатели) (чтобы не занимать врем€)
	data_mutex.lock();
	std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;

	std::vector<LogNote*> ErrorLogs_ = this->ErrorLogs_;
	std::vector<LogNote*> SystemLogs_ = this->SystemLogs_;
	std::vector<LogNote*> SecurityLogs_ = this->SecurityLogs_;
	std::vector<LogNote*> AuthLogs_ = this->AuthLogs_;
	std::vector<LogNote*> ActionLogs_ = this->ActionLogs_;
	std::vector<LogNote*> JudgeLogs_ = this->JudgeLogs_;
	std::vector<LogNote*> NetworkLogs_ = this->NetworkLogs_;
	data_mutex.unlock();

	try {
		// дальше работаем со скопированными данными (на момент запроса)
		vector.clear();

		// временные переменные
		uint32_t uint32_t_buffer;
		time_t time_t_buffer;
		unsigned char uchar_buffer;
		char* tmp_ptr;

		// начинаем перенос
		uint32_t_buffer = AllLogs_data_.size();
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

		for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++) {
			time_t_buffer = AllLogs_data_[i]->time;
			tmp_ptr = reinterpret_cast<char*>(&time_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(time_t_buffer));

			uint32_t_buffer = AllLogs_data_[i]->log_types.size();
			tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

			for (uint32_t g{ 0 }; g < AllLogs_data_[i]->log_types.size(); g++) {
				uchar_buffer = AllLogs_data_[i]->log_types[g];
				vector.push_back(*reinterpret_cast<char*>(&uchar_buffer));
			}

			uint32_t_buffer = AllLogs_data_[i]->log_text.length();
			tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

			tmp_ptr = &AllLogs_data_[i]->log_text[0];
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + AllLogs_data_[i]->log_text.length());
		}

		// ƒл€ ErrorLogs_
		__get_char_other__(vector, ErrorLogs_);

		// ƒл€ SystemLogs_
		__get_char_other__(vector, SystemLogs_);

		// ƒл€ SecurityLogs_
		__get_char_other__(vector, SecurityLogs_);

		// ƒл€ AuthLogs_
		__get_char_other__(vector, AuthLogs_);

		// ƒл€ ActionLogs_
		__get_char_other__(vector, ActionLogs_);

		// ƒл€ JudgeLogs_
		__get_char_other__(vector, JudgeLogs_);

		// ƒл€ NetworkLogs_
		__get_char_other__(vector, NetworkLogs_);
		
		// конец записи
	}
	catch (...) {
		// значит произошла кака€-то ошибка
		vector.clear();

		return false;
	}

	return true;	// значит все данные сохранились в char массив (вектор)
}

void EasyLogs::__get_char_other__(std::vector<char>& vector, const std::vector<LogNote*>& other_vector) {
	uint32_t uint32_t_buffer = other_vector.size();
	char* tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);

	vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));

	for (uint32_t i{ 0 }; i < other_vector.size(); i++) {
		uint32_t_buffer = other_vector[i]->parent_index;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);

		vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t_buffer));
	}
}

bool EasyLogs::OpenViaCharData(const std::vector<char>& vector) {
	// временные переменные
	std::vector<LogNote*> AllLogs_data_;

	std::vector<LogNote*> ErrorLogs_;
	std::vector<LogNote*> SystemLogs_;
	std::vector<LogNote*> SecurityLogs_;
	std::vector<LogNote*> AuthLogs_;
	std::vector<LogNote*> ActionLogs_;
	std::vector<LogNote*> JudgeLogs_;
	std::vector<LogNote*> NetworkLogs_;

	// само чтение с проверкой
	uint32_t data_index{ 0 };
	try {
		AllLogs_data_.reserve(*reinterpret_cast<const uint32_t*>(&vector[data_index]) * 2);
		AllLogs_data_.resize(*reinterpret_cast<const uint32_t*>(&vector[data_index]));
		data_index += sizeof(uint32_t);

		for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++) {
			AllLogs_data_[i] = new LogNote;	// создали

			AllLogs_data_[i]->parent_index = i;	// восстановили логически

			AllLogs_data_[i]->time = *reinterpret_cast<const time_t*>(&vector[data_index]);
			data_index += sizeof(time_t);

			AllLogs_data_[i]->log_types.resize(*reinterpret_cast<const uint32_t*>(&vector[data_index]));
			data_index += sizeof(uint32_t);

			for (uint32_t g{ 0 }; g < AllLogs_data_[i]->log_types.size(); g++) {
				AllLogs_data_[i]->log_types[g] = *reinterpret_cast<const unsigned char*>(&vector[data_index]);
				data_index += sizeof(unsigned char);
			}

			AllLogs_data_[i]->log_text.resize(*reinterpret_cast<const uint32_t*>(&vector[data_index]));
			data_index += sizeof(uint32_t);

			for (uint32_t g{ 0 }; g < AllLogs_data_[i]->log_text.size(); g++) {
				AllLogs_data_[i]->log_text[g] = vector[data_index + g];
			}
			data_index += AllLogs_data_[i]->log_text.length();
		}

		// ƒл€ ErrorLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, ErrorLogs_);

		// ƒл€ SystemLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, SystemLogs_);

		// ƒл€ SecurityLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, SecurityLogs_);

		// ƒл€ AuthLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, AuthLogs_);

		// ƒл€ ActionLogs_ 
		__open_via_char__(vector, data_index, AllLogs_data_, ActionLogs_);

		// ƒл€ JudgeLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, JudgeLogs_);

		// ƒл€ NetworkLogs_
		__open_via_char__(vector, data_index, AllLogs_data_, NetworkLogs_);

		// конец чтени€
	}
	catch (...) {
		// значит произошла кака€-то ошибка (скорее всего битый *файл* или out of range)

		for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++)
			if (AllLogs_data_[i] != nullptr)
				delete AllLogs_data_[i];

		return false;
	}

	// если дошлю сюда - ошибок не было
	Clear();	// очищаем изначальную динамическую пам€ть 

	data_mutex.lock();	// блокируем дл€ переноса
	this->AllLogs_data_ = AllLogs_data_;

	this->ErrorLogs_ = ErrorLogs_;
	this->SystemLogs_ = SystemLogs_;
	this->SecurityLogs_ = SecurityLogs_;
	this->AuthLogs_ = AuthLogs_;
	this->ActionLogs_ = ActionLogs_;
	this->JudgeLogs_ = JudgeLogs_;
	this->NetworkLogs_ = NetworkLogs_;
	data_mutex.unlock();

	return true;
}

void EasyLogs::__open_via_char__(const std::vector<char>& vector, uint32_t& data_index, const std::vector<LogNote*> main_vector, std::vector<LogNote*>& other_vector) {
	other_vector.reserve(*reinterpret_cast<const uint32_t*>(&vector[data_index]) * 2);
	other_vector.resize(*reinterpret_cast<const uint32_t*>(&vector[data_index]));
	data_index += sizeof(uint32_t);

	for (uint32_t i{ 0 }; i < other_vector.size(); i++) {
		other_vector[i] = main_vector[*reinterpret_cast<const uint32_t*>(&vector[data_index])];
		data_index += sizeof(uint32_t);
	}
}

bool EasyLogs::AddLogBack(std::vector<unsigned char> types, std::string text) {
	if (is_open_ == false)
		return false;

	time_t log_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	
	std::sort(types.begin(), types.end(), [](const unsigned char& first, const unsigned char& second) {
		return first < second;
		});

	std::string for_txt{ "" };

	for_txt += __get_str_from_time__(log_time) + ' ';
	for (uint32_t i{ 0 }; i < types.size(); i++)
		for_txt += __get_log_name__(types[i]);
	for_txt += ' ' + text;
	
	if (txt_file_.is_open()) {
		txt_file_mutex.lock();
		txt_file_ << '\n' << for_txt;
		txt_file_.flush();
		txt_file_mutex.unlock();
	}
	
	LogNote* tmp_ptr = new LogNote;
	tmp_ptr->log_text = text;
	tmp_ptr->log_types = types;
	tmp_ptr->time = log_time;
	
	// запрос на бинарник
	data_mutex.lock();
	tmp_ptr->parent_index = AllLogs_data_.size();	// станет на место .back() + 1
	AllLogs_data_.push_back(tmp_ptr);
	for (uint32_t i{ 0 }; i < tmp_ptr->log_types.size(); i++) {
		switch (tmp_ptr->log_types[i])
		{
		case EL_ERROR:
			ErrorLogs_.push_back(tmp_ptr);
			break;
		case EL_SYSTEM:
			SystemLogs_.push_back(tmp_ptr);
			break;
		case EL_SECURITY:
			SecurityLogs_.push_back(tmp_ptr);
			break;
		case EL_AUTH:
			AuthLogs_.push_back(tmp_ptr);
			break;
		case EL_ACTION:
			ActionLogs_.push_back(tmp_ptr);
			break;
		case EL_JUDGE:
			JudgeLogs_.push_back(tmp_ptr);
			break;
		case EL_NETWORK:
			NetworkLogs_.push_back(tmp_ptr);
			break;
		}
	}
	data_mutex.unlock();

	return true;
}

std::string EasyLogs::__get_log_name__(const unsigned char& type) {
	switch (type)
	{
	case EL_ERROR:
		return "[ERROR]";
		break;
	case EL_SYSTEM:
		return "[SYSTEM]";
		break;
	case EL_SECURITY:
		return "[SECURITY]";
		break;
	case EL_AUTH:
		return "[AUTH]";
		break;
	case EL_ACTION:
		return "[ACTION]";
		break;
	case EL_JUDGE:
		return "[JUDGE]";
		break;
	case EL_NETWORK:
		return "[NETWORK]";
		break;
	default:
		return "[NONE]";
		break;
	}
}

std::string EasyLogs::__get_str_from_time__(const time_t& time) {
	std::tm tm_info{};
	localtime_s(&tm_info, &time);

	char buffer[14 + 1];  // 14 символов + '\0'
	strftime(buffer, sizeof(buffer), "%d.%m %H:%M:%S", &tm_info);

	return buffer;
}

void EasyLogs::__put_empty_types__(std::vector<char>& vector) {
	uint32_t uint32_t_buffer = 0;
	char* tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);

	for (uint32_t i{ 0 }; i < 7; i++)
		vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));
}

void EasyLogs::__put_main_data__(std::vector<char>& vector, const std::vector<LogNote*>& main_data, uint32_t first_index, uint32_t last_index) {
	vector.clear();

	// временные переменные
	uint32_t uint32_t_buffer;
	time_t time_t_buffer;
	char* tmp_ptr;

	if (first_index > main_data.size() || last_index > main_data.size()) {
		// значит таких данных просто не может быть
		uint32_t_buffer = 0;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

		return;
	}
	else {
		// значит записываем по логике данные
		uint32_t_buffer = last_index - first_index;
		tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
		vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

		for (uint32_t i{ first_index }; i < last_index; i++) {
			time_t_buffer = main_data[i]->time;
			tmp_ptr = reinterpret_cast<char*>(&time_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(time_t));

			uint32_t_buffer = main_data[i]->log_types.size();
			tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

			for (uint32_t g{ 0 }; g < main_data[i]->log_types.size(); g++)
				vector.push_back(*reinterpret_cast<char*>(&main_data[i]->log_types[g]));

			uint32_t_buffer = main_data[i]->log_text.length();
			tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
			vector.insert(vector.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

			vector.insert(vector.end(), &main_data[i]->log_text[0], &main_data[i]->log_text[0] + main_data[i]->log_text.size());
		}

		return;
	}
}

void EasyLogs::Clear() {
	data_mutex.lock();

	for (uint32_t i{ 0 }; i < AllLogs_data_.size(); i++)
		if (AllLogs_data_[i] != nullptr)
			delete AllLogs_data_[i];

	AllLogs_data_.clear();

	ErrorLogs_.clear();
	SystemLogs_.clear();
	SecurityLogs_.clear();
	AuthLogs_.clear();
	ActionLogs_.clear();
	JudgeLogs_.clear();
	NetworkLogs_.clear();

	data_mutex.unlock();
}

void EasyLogs::print_all() {
	print_all(5000);
}

void EasyLogs::print_all(uint32_t count) {
	if (count > 5000)
		count = 5000;

	if (AllLogs_data_.empty()) {
		__set_cout_color__(DARK_GRAY_COLOR);
		std::cout << "Ћоги пусты" << std::endl;
		__set_cout_color__(WHITE_COLOR);
	}
	else {	// есть что выводить
		print_mutex.lock();

		// временные переменные
		data_mutex.lock();
		std::vector<LogNote*> AllLogs_data_ = this->AllLogs_data_;	// скопировали данные на момент вывода
		data_mutex.unlock();

		uint32_t first_index{ 0 };
		if (AllLogs_data_.size() > count) {
			first_index = AllLogs_data_.size() - count;
			__set_cout_color__(DARK_GRAY_COLOR);
			std::cout << "-------¬ыше еще [" << first_index << "] записей-------\n";
		}

		for (uint32_t i{ first_index }; i < AllLogs_data_.size(); i++) {
			__set_cout_color__(DARK_GRAY_COLOR);
			std::cout << __get_str_from_time__(AllLogs_data_[i]->time) << ' ';

			for (uint32_t g{ 0 }; g < AllLogs_data_[i]->log_types.size(); g++) {
				switch (AllLogs_data_[i]->log_types[g])
				{
				case EL_ERROR:
					__set_cout_color__(RED_COLOR);
					std::cout << "[ERROR]";
					break;
				case EL_SYSTEM:
					__set_cout_color__(LIGHT_MAGENTA_COLOR);
					std::cout << "[SYSTEM]";
					break;
				case EL_SECURITY:
					__set_cout_color__(YELLOW_COLOR);
					std::cout << "[SECURITY]";
					break;
				case EL_AUTH:
					__set_cout_color__(LIGHT_BLUE_COLOR);
					std::cout << "[AUTH]";
					break;
				case EL_ACTION:
					__set_cout_color__(CYAN_COLOR);
					std::cout << "[ACTION]";
					break;
				case EL_JUDGE:
					__set_cout_color__(LIGHT_GREEN_COLOR);
					std::cout << "[JUDGE]";
					break;
				case EL_NETWORK:
					__set_cout_color__(LIGHT_CYAN_COLOR);
					std::cout << "[NETWORK]";
					break;
				default:
					__set_cout_color__(LIGHT_RED_COLOR);
					std::cout << "[NONE]";
					break;
				}
			}

			__set_cout_color__(WHITE_COLOR);
			std::cout << ' ' << AllLogs_data_[i]->log_text << '\n';
		}
		std::cout.flush();	

		print_mutex.unlock();
	}
}

void EasyLogs::__set_cout_color__(uint32_t color_id) {
	if (color_id > 15)
		return;

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color_id);
}