#ifndef  TUPLE_H
#define  TUPLE_H
typedef unsigned int Memory;

#define POINTER_SIZE_BYTES 4


#include "cmap.h"
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>

inline void Split(const std::string &s, char delim, std::vector<std::string>& tokens) {
	std::stringstream ss(s);
	std::string item;
	while (getline(ss, item, delim)) {
		tokens.push_back(item);
	}
}


struct Tuple {
public:
	Tuple(const std::vector<int>& dims, const std::vector<unsigned int>& lengths, const Rule& r) : dims(dims), lengths(lengths) {
		for (int w : lengths) {
			tuple.push_back(w);
		}
		cmap_init(&map_in_tuple);
		Insertion(r);
	}
	//~Tuple() { Destroy(); }
	void Destroy() {
		cmap_destroy(&map_in_tuple);
	}

	bool IsEmpty() { return CountNumRules() == 0; }

	int FindMatchPacket(const Packet& p);
	void Insertion(const Rule& r);
	void Deletion(const Rule& r);
	int CountNumRules() const  {
		return cmap_count(&map_in_tuple);
	//	return  table.size();
	}
	Memory MemSizeBytes(Memory ruleSizeBytes) const {
		return 	cmap_count(&map_in_tuple)* ruleSizeBytes + cmap_array_size(&map_in_tuple) * POINTER_SIZE_BYTES;
		//return table.size() * ruleSizeBytes + table.bucket_count() * POINTER_SIZE_BYTES;
	}

	void printsipdip() {
		//printf("sipdip: %d %d\n", sip_length, dip_length);
	}
protected:
	bool inline IsPacketMatchToRule(const Packet& p, const Rule& r);
	unsigned int inline HashRule(const Rule& r) const;
	unsigned int inline HashPacket(const Packet& p) const;
	cmap map_in_tuple;
	//std::unordered_map<uint32_t, std::vector<Rule>> table;

	std::vector<int> dims;
	std::vector<unsigned int> lengths;
	std::vector<int> tuple;
};

struct PriorityTuple : public Tuple {
public:
	PriorityTuple(const std::vector<int>& dims, const std::vector<unsigned int>& lengths, const Rule& r) :Tuple(dims, lengths, r){
		maxPriority = r.priority;
		priority_container.insert(maxPriority);
	}
	void Insertion(const Rule& r, bool& priority_change);
	void Deletion(const Rule& r, bool& priority_change);

	int maxPriority = -1;
	std::multiset<int> priority_container;
};

class RangeTupleSpaceSearch {

public:
    ~RangeTupleSpaceSearch() {
		for (auto p : all_tuples) {
			p.second.Destroy();
		}
	}

	void ConstructClassifier(const std::vector<Rule>& r);
	int ClassifyAPacket(const Packet& one_packet);
	void DeleteRule(size_t i);
	void InsertRule(const Rule& one_rule);
	void workx(std::string &x) {
		if (x.length() <= 1) {
			spx.clear(); spx.push_back(0); spx.push_back(4); spx.push_back(16);
		}
		else {
			std::vector<std::string> tokens;
			Split(x,',',tokens);
			for (int i = 0; i < tokens.size(); i++) {
				int tx = 0;
				for (int j = 0; j < tokens[i].length(); j++) {
					tx = tx * 10 + tokens[i][j] - '0';
				}
				spx.push_back(tx);
			}
		}
		for (int i = 0; i < spx.size(); i++) {
			printf("%d ", spx[i]);
		}
		printf("\n");
	}
	void worky(std::string &y) {
		if (y.length() <= 1) {
			spy.clear(); spy.push_back(0); spy.push_back(4); spy.push_back(16);
		}
		else {
			std::vector<std::string> tokens;
			Split(y,',',tokens);
			for (int i = 0; i < tokens.size(); i++) {
				int ty = 0;
				for (int j = 0; j < tokens[i].length(); j++) {
					ty = ty * 10 + tokens[i][j] - '0';
				}
				spy.push_back(ty);
			}
		}
		for (int i = 0; i < spy.size(); i++) {
			printf("%d ", spy[i]);
		}
		printf("\n");
	}

	Memory MemSizeBytes() const {
		int ruleSizeBytes = 19; // TODO variables sizes
		int sizeBytes = 0;
		for (auto& pair : rule_tuple) {
			sizeBytes += pair.MemSizeBytes(ruleSizeBytes);
		}
		return sizeBytes;
	}
	void PlotTupleDistribution() {

		std::vector<std::pair<unsigned int, Tuple> > v;
		copy(all_tuples.begin(), all_tuples.end(), back_inserter(v));

		auto cmp = [](const std::pair<unsigned int, Tuple>& lhs, const std::pair<unsigned int, Tuple>& rhs) {
			return lhs.second.CountNumRules() > rhs.second.CountNumRules();
		};
		sort(v.begin(), v.end(), cmp);

		std::ofstream log("logfile.txt", std::ios_base::app | std::ios_base::out);
		int sum = 0;
		for (auto x : v) {
			sum += x.second.CountNumRules();
		}
		log << v.size() << " " << sum << " ";
		int left = 5;
		for (auto x : v) {
			log << x.second.CountNumRules() << " ";
			left--;
			if (left <= 0) break;
		}
		log << std::endl;
	}
	virtual int GetNumberOfTuples() const {
		return rule_tuple.size();
	}
	size_t NumTables() const { return GetNumberOfTuples(); }
	size_t RulesInTable(size_t index) const { return 0; } // TODO : assign some order
protected:
	uint64_t inline KeyRulePrefix(const Rule& r) {
		int key = 0;
		for (int d : dims) {
			key <<= 6;
			key += r.prefix_length[d];
		}
		return key;
	}
	std::unordered_map<uint64_t, Tuple> all_tuples;
	std::vector<Tuple> rule_tuple;
	std::vector<int> rule_map;
	std::vector<int> max_pri;
	//maintain rules for monitoring purpose
	std::vector<Rule> rules;
	std::vector<int> dims;
	std::vector<int> spx,spy; 
};

#endif


