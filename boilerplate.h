#ifndef __BOILERPLATE__
#define __BOILERPLATE__

#include <ctime>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <iomanip>

time_t strtotime_t(std::string const &str) {   
  std::tm date;
  std::istringstream range(str);
  range >> std::get_time(&date, "%Y-%m-%d %H:%M:%S");
  return std::mktime(&date);
}

time_t min_year() {
  static time_t date = strtotime_t("1717-01-01 00:00:00");
  return date;
}

time_t max_year() {
  static time_t date = strtotime_t("4243-01-01 00:00:00");
  return date;
}

class Message {
  private:
    time_t timestamp;
    uint64_t decimal_timestamp;
    char c;
    std::string file_viscera;

    bool error = false;

    static std::string s_buffer;

  public:
    static size_t const MAX_ENTRAILS_SIZE = 65535;
    
    // For client creating the message
    Message(std::string const &timestamp, char c) : c(c) {
      std::istringstream iss(timestamp);
      iss >> this->decimal_timestamp;
      if (iss.fail() || timestamp.compare(std::to_string(this->decimal_timestamp)) != 0) {
        error = true;
        return;
      }
      this->timestamp = static_cast<time_t>(this->decimal_timestamp);
    }

    // Decode message sent on the net
    Message(std::string const &serialized) {
      if (serialized.length() > MAX_ENTRAILS_SIZE || serialized.length() < 9) {
        error = false;
        return;
      }
      this->c = serialized[8];
      file_viscera = serialized.substr(9);
      this->decimal_timestamp = *reinterpret_cast<uint64_t const*>(serialized.c_str());
      if (!is_big_endian()) {
        this->decimal_timestamp = bswap(this->decimal_timestamp);
      }
      this->timestamp = static_cast<time_t>(this->decimal_timestamp);
    }

    bool failed() const {
      return error;
    }

    bool is_correct() const {
      return (!error && std::difftime(max_year(), timestamp) > 0 
              && std::difftime(timestamp, min_year()) >= 0); 
    }

    char const *serialize() const {
      uint64_t ts = this->decimal_timestamp;
      if (!is_big_endian()) {
        ts = bswap(ts);
      }
      char *ts_str = reinterpret_cast<char *>(&ts);
      for (size_t i = 0; i < 8; i++) {
        s_buffer[i] = *(ts_str + i);
      }
      s_buffer[8] = this->c;
      return s_buffer.c_str();
    }

    static uint64_t bswap(uint64_t number) {
      uint64_t const every2b = UINT64_C(0xFF00FF00FF00FF00);
      uint64_t const every2bo = UINT64_C(0xFF00FF00FF00FF);
      uint64_t const every4b = UINT64_C(0xFFFF0000FFFF0000);
      uint64_t const every4bo = UINT64_C(0xFFFF0000FFFF);
      number = ((number << 8) & every2b) | ((number >> 8) & every2bo);
      number = ((number << 16) & every4b) | ((number >> 16) & every4bo);
      number = (number << 32) | (number >> 32);
      return number;
    }
    
    static bool is_big_endian() {
      int const guinea_pig = 1;
      return !(*reinterpret_cast<char const*>(&guinea_pig));
    }
    
    static void initialize_serialization_buffer(std::string const &file_content) {
      s_buffer = std::string(9, ' ') + file_content;
    }

    friend std::ostream &operator<<(std::ostream &os, Message const &ms);

};

std::ostream& operator<<(std::ostream &os, Message const &ms) {
  if (ms.is_correct()) {
    os << ms.decimal_timestamp << " " << ms.c << " " << ms.file_viscera << std::endl;
  }
  return os;
}


#endif /* __BOILERPLATE__ */
