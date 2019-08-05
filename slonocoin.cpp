#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cinttypes>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <array>
#include <chrono>	

#include "BlockFactory.h"
#include "helpers.hpp"

int main() {
	BlockFactory b("tcp://localhost:2000", "InvMiner2", "2");

	b.start_mining();

	while(1) {}

 	return 0;
}