#include "RangeTupleSpaceSearch.h"

// Values for Bernstein Hash
#define HashBasis 5381
#define HashMult 33
int vis[53][53];
void Tuple::Insertion(const Rule& r) {

	cmap_node * new_node = new cmap_node(r); /*key & rule*/
	cmap_insert(&map_in_tuple, new_node, HashRule(r));

	/*uint32_t key = HashRule(r);
	std::vector<Rule>& rl = table[key];
	rl.push_back(r);
	sort(rl.begin(), rl.end(), [](const Rule& rx, const Rule& ry) { return rx.priority >= ry.priority; });*/
}

void Tuple::Deletion(const Rule& r) {
	//find node containing the rule
	unsigned int hash_r = HashRule(r);
	cmap_node * found_node = cmap_find(&map_in_tuple, hash_r);
	while (found_node != nullptr) {
		if (found_node->priority == r.priority) {
			cmap_remove(&map_in_tuple, found_node, hash_r);
			break;
		}
		found_node = found_node->next;
	}

	/*uint32_t key = HashRule(r);
	std::vector<Rule>& rl = table[key];
	rl.erase(std::remove_if(rl.begin(), rl.end(), [=](const Rule& ri) { return ri.priority == r.priority; }), rl.end());*/
}


int Tuple::FindMatchPacket(const Packet& p)  {

	cmap_node * found_node = cmap_find(&map_in_tuple, HashPacket(p));
	int priority = -1;
	while (found_node != nullptr) {
		if (found_node->rule_ptr->MatchesPacket(p)) {
			priority = std::max(priority, found_node->priority);
			break;
		}
		found_node = found_node->next;
	}
	return priority;

/*	auto ptr = table.find(HashPacket(p));
	if (ptr != table.end()) {
		for (const Rule& r : ptr->second) {
			if (r.MatchesPacket(p)) {
				return r.priority;
			}
		}
	}
	return -1;*/
}

bool inline Tuple::IsPacketMatchToRule(const Packet& p, const Rule& r) {
	for (int i = 0; i < r.dim; i++) {
		if (p[i] < r.range[i][LowDim]) return false;
		if (p[i] > r.range[i][HighDim]) return false;
	}
	return true;
}

uint32_t inline Tuple::HashRule(const Rule& r) const {
	uint32_t hash = 0;
	uint32_t max_uint = 0xFFFFFFFF;
	for (size_t i = 0; i < dims.size(); i++) {
		uint32_t mask = lengths[i] != 32 ? ~(max_uint >> lengths[i]) : max_uint;
		hash = hash_add(hash, r.range[dims[i]][LowDim] & mask);
	}
	return hash_finish(hash, 16);

/*	uint32_t hash = HashBasis;
	for (size_t d = 0; d < tuple.size(); d++) {
		hash *= HashMult;
		hash += r.range[d][LowDim] & ForgeUtils::Mask(tuple[d]);
	}
	return hash;*/

}

uint32_t inline Tuple::HashPacket(const Packet& p) const {
	uint32_t hash = 0;
	uint32_t max_uint = 0xFFFFFFFF;

	for (size_t i = 0; i < dims.size(); i++) {
		uint32_t mask = lengths[i] != 32 ? ~(max_uint >> lengths[i]) : max_uint;
		hash = hash_add(hash, p[dims[i]] & mask);
	}
	return hash_finish(hash, 16);

	/*uint32_t hash = HashBasis;
	for (size_t d = 0; d < tuple.size(); d++) {
		hash *= HashMult;
		hash += p[d] & ForgeUtils::Mask(tuple[d]);
	}
	return hash;*/
}
std::vector<int> splitx,splity;
unsigned int cal(unsigned int d, int flag) {
	/*
	if (d >= 32)  return 32;
	else if (d >= 24) return 24;
	else if (d >= 16) return 16;
	else if (d >= 8) return 8;
	else return 0;
	*/
	/*
	if (flag == 0) {
		if (d >= 16) return 16;
		if (d >= 4) return 4;
		return 0;
	} else {
		if (d >= 16) return 16;
		if (d >= 4) return 4;
		return 0;
	}
	*/
	
	if (flag == 0) {
		int tx = splitx.size() - 1;
		for (int i = tx; i >= 0; i--) {
			if (d >= splitx[i]) return splitx[i];
		}
		return 0;
	}
	else {
		int tx = splity.size() - 1;
		for (int i = tx; i >= 0; i--) {
			if (d >= splity[i]) return splity[i];
		}
		return 0;
	}
	
	/*
	if (d >= 16) return 24;
	//if (d >= 10) return 10;
	if (d >= 4) return 4;
	return 0;
	*/
}
int work(const std::vector<unsigned int> &lengths, unsigned int id) {
	if(vis[lengths[0]][lengths[1]] != -1) return vis[lengths[0]][lengths[1]];
	else {
		vis[lengths[0]][lengths[1]] = id;
		return id;
	}

}
void RangeTupleSpaceSearch::ConstructClassifier(const std::vector<Rule>& r){
	/*int multiples = 5;
	for (int i = 0; i < r[0].dim / multiples; i++) {
		dims.push_back(i * multiples);
		dims.push_back(i * multiples + 1);
	}*/
	//splitx = spx; splity = spy;
	//for (int i = 0; i < splitx.size(); i++) printf("%d ",splitx[i]); printf("\n");
	//for (int i = 0; i < splity.size(); i++) printf("%d ",splity[i]); printf("\n");
	memset(vis, -1, sizeof(vis));
	dims.push_back(FieldSA);
	dims.push_back(FieldDA);
	rule_map.reserve(r.size() + 100000);
	rule_tuple.clear();
	for (int i = 0; i < 33*33; i++) {
		max_pri.push_back(-1);
	}
	
	for (const auto& Rule : r) {
		InsertRule(Rule);
	}
	
	//rules = r;
	rules.reserve(100000);
}

int RangeTupleSpaceSearch::ClassifyAPacket(const Packet& packet) {
	int priority = -1;
	for (int i = 0; i < rule_tuple.size(); ++i) {
		if (max_pri[i] == -1 || priority >= max_pri[i]) break;
		auto result = rule_tuple[i].FindMatchPacket(packet);
		priority = std::max(priority, result);
	}
	/*
	int query = 0;
	for (auto& tuple : all_tuples) {
		auto result = tuple.second.FindMatchPacket(packet);
		priority = std::max(priority, result);
		query++;
	}
	QueryUpdate(query);
	*/
	return priority;
}
void RangeTupleSpaceSearch::DeleteRule(size_t i) {

	if (i < 0 || i >= rules.size()) {
		//printf("Warning index delete rule out of bound: do nothing here\n");
		//printf("%lu vs. size: %lu", i, rules.size());
		return;
	}
	int id = rules[i].priority;
	rule_map[id] = -1;
	std::vector<unsigned int> lengths;
	for (int d = 0; d < dims.size(); d++) {
		lengths.push_back(cal(rules[i].prefix_length[d], d));
	}
	id = work(lengths, rule_tuple.size());
	rule_tuple[id].Deletion(rules[i]);

	if (i != rules.size() - 1)
		rules[i] = std::move(rules[rules.size() - 1]);
	rules.pop_back();
}
void RangeTupleSpaceSearch::InsertRule(const Rule& rule) {
	/*
	auto hit = all_tuples.find(KeyRulePrefix(rule));
	if (hit != end(all_tuples)) {
		//there is a tuple
		hit->second.Insertion(rule);
	} else {
		//create_tuple
		std::vector<unsigned int> lengths;
		for (int d : dims) {
			lengths.push_back(cal(rule.prefix_length[d]));
		}
		all_tuples.insert(std::make_pair(KeyRulePrefix(rule), Tuple(dims, lengths, rule)));
	}
	*/
	std::vector<unsigned int> lengths;
	for (int d = 0; d < dims.size(); d++) {
		lengths.push_back(cal(rule.prefix_length[d], d));
	}
	unsigned int id = work(lengths, rule_tuple.size());
	if(max_pri[id] == -1) {
		rule_tuple.push_back(Tuple(dims, lengths, rule));
		max_pri[id] = rule.priority;
	}
	else {
		rule_tuple[id].Insertion(rule);
		max_pri[id] = std::max(max_pri[id], rule.priority);
	}
	rules.push_back(rule);
	rule_map[rule.priority] = rules.size() - 1;
}



