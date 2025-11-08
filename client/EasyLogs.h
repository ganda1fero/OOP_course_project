#ifndef EASYLOGS_H
#define EASYLOGS_H

#define EL_ERROR 1
#define EL_SYSTEM 2
#define EL_SECURITY 3
#define EL_AUTH 4
#define EL_ACTION 5
#define EL_JUDGE 6
#define EL_NETWORK 7

#define BLACK_COLOR 0
#define BLUE_COLOR 1
#define GREEN_COLOR 2
#define CYAN_COLOR 3
#define RED_COLOR 4
#define MAGENTA_COLOR 5
#define YELLOW_COLOR 6
#define LIGHT_GRAY_COLOR 7
#define DARK_GRAY_COLOR 8
#define LIGHT_BLUE_COLOR 9
#define LIGHT_GREEN_COLOR 10
#define LIGHT_CYAN_COLOR 11
#define LIGHT_RED_COLOR 12
#define LIGHT_MAGENTA_COLOR 13
#define LIGHT_YELLOW_COLOR 14
#define WHITE_COLOR 15

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <Windows.h>

class EasyLogs {
public:
	EasyLogs();
	EasyLogs(std::string name);
	EasyLogs(std::vector<char> data);
	~EasyLogs();

	bool open(std::string name);
	bool open_via_char(std::vector<char> data);
	
	bool is_open();

	bool create(std::string name);
	
	bool save();
	bool save_as(std::string name);

	void close();

	void select_all(std::vector<char>& vector);
	void select_all(unsigned char type, std::vector<char>& vector);
	void select_from(time_t time_from, std::vector<char>& vector);
	void select_from(unsigned char type, time_t time_from, std::vector<char>& vector);
	void select_to(time_t time_to, std::vector<char>& vector);
	void select_to(unsigned char type, time_t time_to, std::vector<char>& vector);
	void select_from_to(time_t time_from, time_t time_to, std::vector<char>& vector);
	void select_from_to(unsigned char type, time_t time_from, time_t time_to, std::vector<char>& vector);

	bool insert(const unsigned char& type, const std::string& text);
	bool insert(const unsigned char& type1, const unsigned char& type2, const std::string& text);
	bool insert(const unsigned char& type1, const unsigned char& type2, const unsigned char& type3, const std::string& text);

	void print_all();
	void print_all(uint32_t count);
	void __set_cout_color__(uint32_t color_id);

private:
	std::mutex data_mutex;
	std::mutex txt_file_mutex;
	std::mutex print_mutex;

	struct LogNote {
		time_t time = -1;						// время лога
		std::vector<unsigned char> log_types;	// типы лога
		std::string log_text = "";				// текст лога
		uint32_t parent_index = 0;				// расположение в общей памяти
	};
	
	std::string logs_name_;	// имя ллгов

	std::ofstream txt_file_;// текстовый файл логов

	bool is_open_;
	
	//---
	std::vector<LogNote*> AllLogs_data_;

	std::vector<LogNote*> ErrorLogs_;
	std::vector<LogNote*> SystemLogs_;
	std::vector<LogNote*> SecurityLogs_;
	std::vector<LogNote*> AuthLogs_;
	std::vector<LogNote*> ActionLogs_;
	std::vector<LogNote*> JudgeLogs_;
	std::vector<LogNote*> NetworkLogs_;
	//---

	bool ReadFromFile();
	void __read_other_logs__(std::ifstream& file, const std::vector<LogNote*>& main_vector, std::vector<LogNote*>& other_vector);

	bool SaveToFile();

	bool GetCharAllData(std::vector<char>& vector);
	void __get_char_other__(std::vector<char>& vector, const std::vector<LogNote*>& other_vector);

	bool OpenViaCharData(const std::vector<char>& vector);
	void __open_via_char__(const std::vector<char>& vector, uint32_t& data_index, const std::vector<LogNote*> main_vector, std::vector<LogNote*>& other_vector);

	bool AddLogBack(std::vector<unsigned char> types, std::string text);
	std::string __get_log_name__(const unsigned char& type);
	std::string __get_str_from_time__(const time_t& time);

	void __put_empty_types__(std::vector<char>& vector);
	void __put_main_data__(std::vector<char>& vector, const std::vector<LogNote*>& main_data, uint32_t first_index, uint32_t last_index);
	
	void Clear();
};


#endif