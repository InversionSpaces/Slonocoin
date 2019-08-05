#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <set>
#include <vector>
#include <algorithm>

#include "openssl/sha.h"

#include "boost_1_70_0/boost/multiprecision/cpp_int.hpp"
#include "json/single_include/nlohmann/json.hpp"

#include "BlockFactory.h"

typedef boost::multiprecision::uint256_t uint256_t;
typedef std::string hash_t;
typedef std::vector<hash_t> hash_set_t;
typedef nlohmann::json json;

template<typename T>
inline std::string to_string(const T& val) {
	std::stringstream ss;

	ss << val;

	return ss.str();
}

template<typename T>
inline std::string to_hex(const T& val) {
	std::stringstream ss;

	ss << std::hex 
	<< std::setfill ('0') 
	<< std::setw(SHA256_DIGEST_LENGTH * 2)
	<< val;

	return ss.str();
}

template<typename T>
inline T from_string(const std::string& s) {
	std::stringstream ss;
	T retval;

	ss << s;
	ss >> retval;

	return retval;
}

/*
template<typename T>
inline T from_hex(const std::string& val) {
	std::stringstream ss;
	ss << val;
	ss << std::hex;

	T retval;
	ss >> retval;
	return retval;
}
*/

inline hash_t hash_str(const std::string& line) {    
    uint8_t hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, line.c_str(), line.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    ss << std::hex << std::setfill ('0');

    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::setw(2) << int(hash[i]);

    return ss.str();
}

inline hash_t hash_adding(const hash_set_t& hashes, const hash_t& hash) {
	std::stringstream ss;

	auto upper = std::upper_bound(hashes.begin(), hashes.end(), hash);
	for(auto it = hashes.begin(); it != hashes.end(); it++) {
		if(it == upper) ss<<hash;
		ss<<*it;
	}
	if(upper == hashes.end()) ss<<hash;

	return hash_str(ss.str());
}

inline hash_t hash_set(const hash_set_t& hs) {
	std::stringstream ss;

	for(const hash_t& h: hs) ss<<h;

	return hash_str(ss.str());
}

template<typename T>
inline hash_t hash(const T& val) {
	return hash_str(to_string(val));
}

#endif