#ifndef BLOCK_FACTORY_H
#define BLOCK_FACTORY_H

#include <iostream>
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
#include <thread>
#include <algorithm>
#include <atomic>
#include <shared_mutex>
#include <deque>

#include "helpers.hpp"

#include "mqtt/async_client.h"

struct Transaction {
	hash_t from;
	hash_t to;
	std::string charge;
};

struct Block {
	uint256_t 					id;
	uint256_t 					nonce;
	hash_t 						prev_hash;
	hash_t 						threshold;
	std::string 				ver;
	std::vector<Transaction> 	transactions;
};

inline std::string block_to_json_str(Block b) {
	json j;
	j["id"] = to_string(b.id);
	j["nonce"] = to_string(b.nonce);
	j["prev_hash"] = b.prev_hash;
	j["threshold"] = b.threshold;
	j["ver"] = b.ver;
	for(int i = 0; i < b.transactions.size(); i++) {
		j["transactions"][i]["from"] = b.transactions[i].from;
		j["transactions"][i]["to"] = b.transactions[i].to;
		j["transactions"][i]["charge"] = b.transactions[i].charge;
	}
	return j.dump();
}

inline Block block_from_json(const json& j) {
	Block b;
	b.id = from_string<uint256_t>(j.at("id").get<std::string>());
	b.nonce = from_string<uint256_t>(j.at("nonce").get<std::string>());
	b.prev_hash = j.at("prev_hash").get<hash_t>();
	b.threshold = j.at("threshold").get<hash_t>();
	b.ver = j.at("ver").get<std::string>();
	for(int i = 0; j.contains("transactions") && i < j.at("transactions").size(); i++) {
		Transaction t;
		t.from = j.at("transactions")[i].at("from").get<hash_t>();
		t.to = j.at("transactions")[i].at("to").get<hash_t>();
		t.charge = j.at("transactions")[i].at("charge").get<std::string>();
		b.transactions.push_back(t);
	}
	return b;
}

inline Transaction transaction_from_json(const json& j) {
	Transaction t;
	t.from = j.at("from").get<hash_t>();
	t.to = j.at("to").get<hash_t>();
	t.charge = j.at("charge").get<std::string>();
	return t;
}

inline std::string transaction_to_json_str(Transaction t) {
	json j;
	j["from"] = t.from;
	j["to"] = t.to;
	j["charge"] = t.charge;
	return j.dump();
}

inline hash_t hash(const Block& b) {
	hash_set_t hashes;
	hashes.push_back(hash(b.id));
	hashes.push_back(hash(b.nonce));
	hashes.push_back(hash_str(b.prev_hash));
	hashes.push_back(hash_str(b.threshold));
	hashes.push_back(hash_str(b.ver));
	for(int i = 0; i < b.transactions.size(); i++) {
		hashes.push_back(hash_str(b.transactions[i].to));
		hashes.push_back(hash_str(b.transactions[i].from));
		hashes.push_back(hash_str(b.transactions[i].charge));
	}
	std::sort(hashes.begin(), hashes.end());
	return hash_set(hashes);
}

class BlockFactory {
private:
	std::vector<Block> chain;

	std::atomic<bool> working;

	struct {
		std::deque<Block> que;
		std::mutex mutex;
		std::condition_variable cv;
	} recv_block;

	struct 
	{
		std::deque<Block> que;
		std::mutex mutex;
		std::condition_variable cv;
	} send_block;

	struct {
		std::deque<Transaction> que;
		std::mutex mutex;
		std::condition_variable cv;
	} recv_transaction;

	struct 
	{
		std::deque<Transaction> que;
		std::mutex mutex;
		std::condition_variable cv;
	} send_transaction;

	struct {
		Block block;
		hash_set_t hashes;
		mutable std::shared_mutex mutex;
	} current;

	std::vector<std::thread> threads;

	void start_threads();

public:

	class BlockFactoryMQTTClient {
		class BlockFactoryCallback : public virtual mqtt::callback
		{
			const int	QOS = 1;
			const std::vector<std::string> topics = {"blocks", "transactions"};

			int nretry_;

			BlockFactory& blockfactory;
			mqtt::async_client& mqttclient;

			void connected(const std::string& cause) override {
				std::cout<<"Connected"<<std::endl;
				for(const std::string& t: topics)
					mqttclient.subscribe(t, QOS);
			}

			void connection_lost(const std::string& cause) override {
				std::cout<<"Connection loss"<<std::endl;
			}

			void message_arrived(mqtt::const_message_ptr msg) override {
				std::cout<<"Message arrived"<<std::endl;
				std::string topic = msg->get_topic();
				std::string payload = msg->get_payload_str();
				if(topic == "blocks")
					blockfactory.block_arrived(payload);
				else if(topic == "transactions")
					blockfactory.transaction_arrived(payload);
			}

			public:
			BlockFactoryCallback(BlockFactory& bf, mqtt::async_client& mc) : 
				nretry_(0), 
				blockfactory(bf),
				mqttclient(mc) {}
		} callback;

		const std::string uri;
		const std::string id;

		mqtt::async_client client;
	public:
		void send(Block b);
		void send(Transaction t);
		void reconnect();
		void connect();
		bool is_connected();

		BlockFactoryMQTTClient(
					const std::string& uri, 
					const std::string& id, 
					BlockFactory& bf) : 
			uri(uri), id(id),
			client(uri, id),
			callback(bf, client) {
				client.set_callback(callback);
			}
	} mqtt;

	BlockFactory(	const std::string& uri, 
					const std::string& id, 
					const std::string ver = "0.0.1");

	void start_mining();

	void block_arrived(const std::string& block);
	void transaction_arrived(const std::string& trans);

	~BlockFactory() = default;
};

#endif