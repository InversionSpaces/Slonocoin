#include "BlockFactory.h"


void BlockFactory::BlockFactoryMQTTClient::send(Block b) {
	mqtt::message_ptr pubmsg = mqtt::make_message("blocks", block_to_json_str(b));
	client.publish(pubmsg);
}

void BlockFactory::BlockFactoryMQTTClient::send(Transaction t) {
	mqtt::message_ptr pubmsg = mqtt::make_message("transactions", transaction_to_json_str(t));
	client.publish(pubmsg);
}

void BlockFactory::BlockFactoryMQTTClient::reconnect() {
	client.reconnect();
}

void BlockFactory::BlockFactoryMQTTClient::connect() {
	client.connect();
}

bool BlockFactory::BlockFactoryMQTTClient::is_connected() {
	return client.is_connected();
}

void BlockFactory::start_threads() {
	auto miner = [&w = working, &cur = current,
		&sendb = send_block] 
		(uint256_t down, uint256_t delta) {
		for(uint256_t i = 0; w.load(); i = (i + 1) % delta) {
			
			cur.mutex.lock_shared();
			hash_set_t hashes = cur.hashes;
			Block block = cur.block;
			cur.mutex.unlock_shared();

			hash_t h = hash(i + down);
			hash_t blockhash = hash_adding(hashes, h);

			if(blockhash < block.threshold) {
				block.nonce = i + down;

				sendb.mutex.lock();
				sendb.que.push_back(block);
				sendb.mutex.unlock();

				sendb.cv.notify_one();
			}
		}
	};

	auto block_sender = [&w = working,
		&sendb = send_block, &mqtt = mqtt]
	() {
		for(;w.load();) {
			std::unique_lock<std::mutex> lock(sendb.mutex);
			sendb.cv.wait(lock, [&sendb] () {return !sendb.que.empty();});

			while(!sendb.que.empty()) {
				mqtt.send(sendb.que.front());
				sendb.que.pop_front();
			}
		}
	};

	auto transaction_sender = [&w = working,
		&sendt = send_transaction, &mqtt = mqtt]
	() {
		for(;w.load();) {
			std::unique_lock<std::mutex> lock(sendt.mutex);
			sendt.cv.wait(lock, [&sendt] () {return !sendt.que.empty();});

			while(!sendt.que.empty()) {
				mqtt.send(sendt.que.front());
				sendt.que.pop_front();
			}
		}
	};

	auto block_receiver = [&w = working,
		&recvb = recv_block, &cur = current]
	() {
		for(;w.load();) {
			std::unique_lock<std::mutex> lock(recvb.mutex);
			recvb.cv.wait(lock, [&recvb] () {return !recvb.que.empty(); });

			while(!recvb.que.empty()) {
				Block b = recvb.que.front();
				recvb.que.pop_front();

				hash_t h = hash(b);
				if((h < b.threshold) && (b.id == cur.block.id)) {
					cur.mutex.lock();

					cur.block.id += 1;
					cur.block.prev_hash = h;
					cur.block.transactions.clear();

					cur.hashes.clear();

					cur.hashes.push_back(hash(cur.block.id));
					cur.hashes.push_back(hash(cur.block.prev_hash));
					cur.hashes.push_back(hash(cur.block.ver));
					cur.hashes.push_back(hash(cur.block.threshold));

					std::sort(cur.hashes.begin(), cur.hashes.end());

					cur.mutex.unlock();
				}	
			}
		}
	};

	auto transaction_receiver = [&w = working,
		&recvt = recv_transaction, &cur = current] 
	() {
		for(;w.load();) {
			std::unique_lock<std::mutex> lock(recvt.mutex);
			recvt.cv.wait(lock, [&recvt] () {return !recvt.que.empty(); });

			while(!recvt.que.empty()) {
				Transaction t = recvt.que.front();
				recvt.que.pop_front();

				cur.mutex.lock();

				cur.block.transactions.push_back(t);

				cur.hashes.push_back(hash(t.from));
				cur.hashes.push_back(hash(t.to));
				cur.hashes.push_back(hash(t.charge));

				std::sort(cur.hashes.begin(), cur.hashes.end());

				cur.mutex.unlock();
			}
		}
	};

	working = true;
	
	threads.emplace_back(block_receiver);
	threads.emplace_back(block_sender);
	threads.emplace_back(transaction_sender);
	threads.emplace_back(transaction_receiver);

	int nts = 4/*std::thread::hardware_cuncurrency()*/;
	uint256_t delta = std::numeric_limits<uint256_t>::max() / nts;
	for(int i = 0; i < nts; i++)
		threads.emplace_back(miner, i * delta, delta);
}

void BlockFactory::start_mining() {
	std::cout<<"Strarting"<<std::endl;

	mqtt.connect();

	while(!mqtt.is_connected()) {
		std::cout<<"Trying to connect"<<std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

	std::cout<<"Mining with threshold: " << current.block.threshold << std::endl;

	start_threads();
}

BlockFactory::BlockFactory(
		const std::string& uri, 
		const std::string& id, 
		std::string ver) :
	mqtt(uri, id, *this) {

	current.block.id = 0;
	current.block.prev_hash = "";
	current.block.nonce = 0;
	uint256_t th = std::numeric_limits<uint256_t>::max();
	for(int i = 0; i < 1; i++)
		th = th / uint256_t(std::numeric_limits<uint16_t>::max());
	current.block.threshold = to_hex(th);
	current.block.ver = ver;

	current.hashes.push_back(hash(current.block.id));
	current.hashes.push_back(hash(current.block.prev_hash));
	current.hashes.push_back(hash(current.block.ver));
	current.hashes.push_back(hash(current.block.threshold));

	std::sort(current.hashes.begin(), current.hashes.end());	
}

void BlockFactory::block_arrived(const std::string& block) {
	Block b;
	try {
		b = block_from_json(nlohmann::json::parse(block));
	}
	catch (std::exception& e) {
		std::cout<<"Exception "<<e.what()<<std::endl;
		return;
	}

	recv_block.mutex.lock();
	recv_block.que.push_back(b);
	recv_block.mutex.unlock();
	recv_block.cv.notify_one();
}

void BlockFactory::transaction_arrived(const std::string& trans) {
	Transaction t;
	std::cout<<"Transaction "<<trans<<std::endl;
	try {
		t = transaction_from_json(nlohmann::json::parse(trans));
	}
	catch (std::exception& e) {
		std::cout<<"Exception "<<e.what()<<std::endl;
		return;
	}

	recv_transaction.mutex.lock();
	recv_transaction.que.push_back(t);
	recv_transaction.mutex.unlock();
	recv_transaction.cv.notify_one();
}
