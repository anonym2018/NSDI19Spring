#include "RangeTupleSpaceSearch.h"
#include "getopt.h"

using namespace std;
#define uint32 uint32_t

vector<Packet> ReadPacket(char *filename){
  vector<Packet> packet;
	FILE *fp; fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Could not open filter set file\n"); exit(1);
  }
  char str[120]; 
  uint32_t tp[6];
  while(fgets(str, 120, fp) != NULL) {
    Packet p;
    sscanf(str, "%u%u%u%u%u%u", &tp[0], &tp[1], &tp[2], &tp[3], &tp[4], &tp[5]);
    for (int i = 0; i < 5; ++i) p.push_back(tp[i]);
    packet.push_back(p);
  }
  return packet;
}

void ReadIpRange(uint32_t *tmprange, uint32_t mask, array<uint32_t, 2> &IPrange) {
  uint32_t mask3; int mask1, mask2;
  mask1=mask/8; mask2=mask%8;
  int i;
  for (i = 3;i > 3-mask1; --i) tmprange[i] = 0;
  if (mask2 != 0) {
    mask3 = 1; mask3 <<= mask2; mask3 -= 1;
    mask3 = ~mask3;
    tmprange[3-mask1] &= mask3;
  }
  for (i = 0; i <= 3; ++i) {
    if (i == 0) {
      IPrange[0] = tmprange[i];
    } else {
      IPrange[0] <<= 8;
      IPrange[0] += tmprange[i];
    }
  }
  for (i = 3; i > 3-mask1; --i) tmprange[i] = 255;
  if (mask2 != 0) {
    mask3 = 1; mask3 <<= mask2; mask3 -= 1;
    tmprange[3-mask1] |= mask3;
  }
  for (i = 0; i <= 3; ++i) {
    if (i == 0) {
      IPrange[1] = tmprange[i];
    }
    else{
      IPrange[1] <<= 8;
      IPrange[1] += tmprange[i];
    }
  }
}
void ReadRule(char *str, Rule &r) {
  int len = strlen(str);
  int i;
  for(i = 0; i < 5; ++i)
    r.range[i][0] = r.range[i][1] = r.prefix_length[i] = 0;
  uint32_t tmp[4];
  int cnt = 0, val = 0;
  for (i = 1; i < len; ++i) {
    if (str[i] == '.') {
      tmp[cnt++] = val; val = 0;
    } else if(str[i] == '/') {
      tmp[cnt++] = val; val = 0;
      i++;
      break;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  for ( ; i < len; ++i) {
    if (str[i] == '\t' || str[i] == ' ') {
      r.prefix_length[0] = val; val = 0; ++i;
      ReadIpRange(tmp, 32-r.prefix_length[0], r.range[0]);
      break;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  while (str[i] == ' ') ++i;

  cnt = 0; val = 0;
  for ( ; i < len; ++i) {
    if (str[i] == '.') {
      tmp[cnt++] = val; val = 0;
    } else if(str[i] == '/') {
      tmp[cnt++] = val; val = 0;
      i++;
      break;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  for ( ; i < len; ++i){
    if (str[i] == '\t' || str[i] == ' ') {
      r.prefix_length[1] = val; val = 0; ++i;
      ReadIpRange(tmp, 32-r.prefix_length[1], r.range[1]);
      break;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  while (str[i] == ' ') ++i;

  cnt = 0; val = 0;
  for ( ; i < len; ++i) {
    if (str[i] == '\t' || str[i] == ' ') {
      if (val != 0) {
        r.range[2][cnt] = val;
        if (cnt == 1) { ++i; break; }
      }
      val = 0;
    } else if (str[i] == ':') {
      ++cnt;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  while (str[i] == ' ') ++i;

  cnt = 0; val = 0;
  for ( ; i < len; ++i) {
    if (str[i] == '\t' || str[i] == ' ') {
      if (val != 0){
        r.range[3][cnt] = val;
        if (cnt == 1) { ++i; break; }
      }
      val = 0;
    } else if (str[i] == ':') {
      ++cnt;
    } else {
      val = val*10 + str[i] - '0';
    }
  }
  while (str[i] == ' ') ++i;

  for ( ; i < len; ++i) {
    if (str[i] == '/') {
      if (str[i+3] != 'F' || str[i+4] != 'F') {
        r.range[4][0] = 0; r.range[4][1] = 255;
      } else {
        if (str[i-2] >= 'A' && str[i-2] <= 'Z') val = (str[i-2] - 'A' + 10);
        else val = str[i-2] - '0';
        val = val*16;
        if (str[i-1] >= 'A' && str[i-1] <= 'Z') val += (str[i-1] - 'A' + 10);
        else val += str[i-1] - '0';
        r.range[4][0] = r.range[4][1] = val;
      }
      break;
    }
  }
}
vector<Rule> ReadFilterFile(char *filename){
	vector<Rule> rules;
	FILE *fp; fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Could not open filter set file\n"); exit(1);
  }
  char str[120]; array<uint32_t, 2> range[5];
  int line_number=0;
  Rule rule(5);
  while(fgets(str, 120, fp) != NULL) {
    ReadRule(str, rule);
    line_number++;
    rules.push_back(rule);
  }
  for (int i = 0; i < rules.size(); i++){
	rules[i].priority = --line_number;
  }
  return rules;
}
/*
void workopt(char *str, int flag) {
	int num = 0;
	//printf("str = %s flag = %d\n", str, flag);
	char *sp, *tok;
	if (flag == 1) {
		for (; ; str = NULL) {
			tok = strtok_r(str, ",", &sp);
			if (tok == NULL) break;
			splitx[++num] = strtoul(tok, NULL, 0);
		}
		splitx[0] = num;

		//for (int i = 0; i < num; i++) printf(" %d ", splitx[i]); 
		//printf("\n");
	} else {
		for (; ; str = NULL) {
			tok = strtok_r(str, ",", &sp);
			if (tok == NULL) break;
			splity[++num] = strtoul(tok, NULL, 0);
		}
		splity[0] = num;
		//for (int i = 0; i < num; i++) printf(" %d ", splity[i]); 
		//printf("\n");
	}
}
*/
int main(int argc, char* argv[]){

	char rule_file[20], pac_file[20];
	char strx[100], stry[100];

	char short_opts[] = "r:p:x:y:";
	int opt;
	while((opt = getopt(argc, argv, short_opts))) {
		if (opt == 'r') {
			printf("%s\n", optarg);
			strcpy(rule_file, optarg);
		} else if (opt == 'p') {
			printf("%s\n", optarg);
			strcpy(pac_file, optarg);
		} 
		
		else if (opt == 'x') {
		    strcpy(strx, optarg);
			//workopt(strx, 1);
		} else if (opt == 'y') {
			strcpy(stry, optarg);
			//workopt(str, 2);
		}
		
		if (opt == 'y') break;
		
	}
	
    vector<Rule> rules=ReadFilterFile(rule_file);
    cout<<"size="<<rules.size()<<endl;
	vector<Packet> packet=ReadPacket(pac_file);
	cout<<"packet="<<packet.size()<<endl;

	printf("now begin classifier\n");
    RangeTupleSpaceSearch classifier;

	std::string tx,ty;
	tx = strx; ty = stry;
	classifier.workx(tx);
	classifier.worky(ty);

    classifier.ConstructClassifier(rules);
    vector<int> results;
	
  struct timespec tp_a, tp_b;
  clock_gettime(CLOCK_MONOTONIC, &tp_b);
    for(auto const &p: packet){
        int ans=classifier.ClassifyAPacket(p);
        results.push_back(ans);
    }
  clock_gettime(CLOCK_MONOTONIC, &tp_a);
  printf("time: begin %lu s, %lu ns\n", tp_b.tv_sec, tp_b.tv_nsec);
  printf("time: end   %lu s, %lu ns\n", tp_a.tv_sec, tp_a.tv_nsec);
  long total_time = ((long)tp_a.tv_sec - (long)tp_b.tv_sec)*1000000000ULL +
                    (tp_a.tv_nsec - tp_b.tv_nsec);
  printf("total time = %ld ns\n", total_time);
  double ave_time = (double)total_time/packet.size();
  double gbps = (1.0/ave_time)*1e9/1.48e6;
  printf("ave time = %.2lf ns\n", ave_time);
  printf("gbps = %.2lf gbps\n", gbps);

 /*
    vector<int> results2;
    for(auto const &p: packet) {
      int ans = -1;
      for(auto const &r: rules) {
        if (r.MatchesPacket(p)) {
          ans = max(ans, r.priority);
        }
      }
      results2.push_back(ans);
    }
    
  for(int i = 0; i < results.size(); ++i) {
    if(results[i] != results2[i])
      printf("i = %d ans = %d %d\n", i ,results[i], results2[i]);
  }
  */
  return 0;
}


