#include<iostream>
#include "boilerplate.h"

int main() {
  std::istringstream ranges("1717-01-01 00:00:00 4243-01-01 00:00:00");
  std::tm b, e;
  ranges >> std::get_time(&b, "%Y-%m-%d %H:%M:%S");
  ranges >> std::get_time(&e, "%Y-%m-%d %H:%M:%S");
  std::cout << (std::mktime(&b) > std::time(nullptr)) << (std::time(nullptr) < std::mktime(&e)) << std::endl;
  return 0;
}
