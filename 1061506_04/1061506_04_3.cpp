#include <iostream>
#include <fstream>
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <string>
#include <iterator>
#include <functional>
#include <iomanip>
using namespace std;

const bool debug_ = false;

using gid_t_ = int;

const int R_NUM = 5; // resource number
using R_t = array<int, R_NUM>; // resource type

R_t available_;

enum { MAX_, ALLOC_, REQ_, AVAILABLE_, };
const int ARR_NUM_ = ALLOC_ + 1;
const string ARR_NAME_[] = {
	"#MAX",
	"#ALLOCATION",
	"#REQUEST",
	"#AVAILABLE",
};
const int MAX_GID_ = 100;
int gid_num;

struct req_t {
	using op_t = string;
	gid_t_ gid = -1;
	R_t resources = {};
	op_t op;
	size_t size() const {
		return R_NUM + 1;
	}
	int operator[](int i) const {
		if (!i)
			return gid;
		else
			return resources[i - 1];
	}
};
const req_t::op_t op_alloc = "allocate";
const req_t::op_t op_rels = "release";
R_t avail;
map<gid_t_, R_t> datas[ARR_NUM_];
vector<req_t> reqs, waiting_q;

using seq_t = vector<int>;

template<typename C>
struct print_all {
	C const& cont_;
	string begin_, sep_, end_;
	print_all(C const& cont)
		: print_all::print_all(cont, "(", ", ", ")")
	{}

	print_all(C const& cont, string begin, string sep, string end)
		: cont_(cont), begin_(begin), sep_(sep), end_(end)
	{}

	friend ostream& operator<<(ostream& os, print_all const& pa) {
		os << pa.begin_;
		for (int i = 0; i < pa.cont_.size(); ++i) {
			if (i)
				os << pa.sep_;
			os << setw(2) << pa.cont_[i];
		}
		os << pa.end_;
		return os;
	}
};

template<typename C>
inline print_all<C> print_all_(C const& cont) {
	return print_all<C>(cont);
}

void init_(string const& file) {
	ifstream fin(file);
	int64_t state_ = -1, c;
	string s;
	while ((c = fin.peek()) != EOF) {
		if (c == '/') {
			if(debug_)
				clog << "comment\n";
			getline(fin, s);
		}
		else if (c == '#') {
			getline(fin, s);
			state_ = find(begin(ARR_NAME_), end(ARR_NAME_), s) - begin(ARR_NAME_);
			if (debug_)
				clog << "array " << s << ' ' << state_ << "\n";
			if (state_ == 4) {
				cerr << "invalid array name \"" << s << "\"\n";
				exit(EXIT_FAILURE);
			}
		}
		else {
			if (debug_)
				clog << "getline\n";
			if (state_ == -1) {
				cerr << "invalid input format: missing array name start with \"#\"\n";
				exit(EXIT_FAILURE);
			}
			else if (state_ == AVAILABLE_) {
				for (auto& r : avail)
					fin >> r;
			}
			else if (state_ == REQ_) {
				int gid;
				R_t rs;
				req_t::op_t op;
				fin >> gid;
				for (auto& r : rs)
					fin >> r;
				fin >> op;
				if (op != "a" && op != "r") {
					cerr << "invalid request operation " << op
						<< "\nop must be a or r\n";
					exit(EXIT_FAILURE);
				}
				else if (op == "a")
					op = op_alloc;
				else
					op = op_rels;
				reqs.push_back({ gid, rs, op });
			}
			else {
				int gid;
				fin >> gid;
				for (auto& r : datas[state_][gid])
					fin >> r;
				if (state_ == MAX_)
					++gid_num;
			}
			getline(fin, s);
		}
	}
}

R_t& operator+=(R_t& a, R_t const& b) {
	for (int i = 0; i < a.size(); ++i)
		a[i] += b[i];
	return a;
}

R_t operator+(R_t const& a, R_t const& b) {
	R_t tmp = a;
	return tmp += b;
}

R_t& operator-=(R_t& a, R_t const& b) {
	for (int i = 0; i < a.size(); ++i)
		a[i] -= b[i];
	return a;
}

R_t operator-(R_t const& a, R_t const& b) {
	R_t tmp = a;
	return tmp -= b;
}

bool operator<=(R_t const& a, R_t const& b) {
	auto it = b.begin();
	return all_of(begin(a), end(a),
		[&it](auto r)->bool {
		return r <= *it++;
	});
}

seq_t is_safety_(R_t av, map<gid_t_, R_t> al, map<gid_t_, R_t> max, req_t rq = {}) {
	map<gid_t_, bool> fin{};
	seq_t safe_s;
	auto need = map<gid_t_, R_t>{};
	for (auto const&[id, r] : al) {
		fin[id] = false;
		auto const& check = (need[id] = max[id] - r);
		if (any_of(begin(check), end(check),
			[](auto r)->bool { return r < 0; })) {
			cerr << "Error: gid " << id << " max < allocate\n"
				"     max: " << print_all_(max[id]) << "\n"
				"allocate: " << print_all_(al[id]) << "\n\n";
			exit(EXIT_FAILURE);
		}
	}

	if (~rq.gid)
			cout << print_all_(rq) << ": AVAILABLE = " << print_all_(av) << '\n';
	while (any_of(begin(fin), end(fin),
		[](auto p)->bool { return p.second == false; })) {
		int gid = -1;
		R_t res{};
		for (auto const&[id, r] : need)
			if (!fin[id] && r <= av) {
				gid = id;
				res = r;
				break;
			}
		if (!~gid)
			break;
		av -= res;
		auto alloc = al[gid] + res;
		if (~rq.gid)
			cout << print_all_(rq) << ": gid " << gid << " execute: -" << print_all_(res) << "   AVAILABLE = " << print_all_(av) << '\n';
		av += alloc;
		fin[gid] = true;
		if (~rq.gid)
			cout << print_all_(rq) << ": gid " << gid << " finish:  +" << print_all_(alloc) << "   AVAILABLE = " << print_all_(av) << '\n';
		safe_s.push_back(gid);
	}

	return move(safe_s);
}

void determine_init() {
	seq_t ss;
	ss = is_safety_(avail, datas[ALLOC_], datas[MAX_]);
	cout << "Initial state: ";
	if (ss.size() < gid_num)
		cout << "unsafe\n";
	else
		cout << "safe, safe sequence = " << print_all_(ss) << '\n';
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		cerr << "No input files or too many input files!\n";
		exit(EXIT_FAILURE);
	}
	init_(argv[1]);
	
	determine_init();
	
	seq_t ss;
	for (auto& rq : reqs) {
		cout << "gid " << rq.gid << ' ' << rq.op
			<< ' ' << print_all_(rq.resources) << ":\n";
		if (rq.op == op_alloc) {
			if (rq.resources <= datas[MAX_][rq.gid] - datas[ALLOC_][rq.gid]) {
				if (rq.resources <= avail) {
					avail -= rq.resources;
					datas[ALLOC_][rq.gid] += rq.resources;
					ss = is_safety_(avail, datas[ALLOC_], datas[MAX_], rq);
					if (ss.size() == gid_num)
						cout << "granted, safe sequence = " << print_all_(ss) << '\n';
					else {
						avail += rq.resources;
						datas[ALLOC_][rq.gid] -= rq.resources;
						cout << "unsafe, must wait\n";
						waiting_q.push_back(rq);
					}
				}
				else {
					cout << "not enough resouces, must wait\n";
					waiting_q.push_back(rq);
				}
			}
			else {
				cout << "invalid request, not granted\n";
			}
		}
		else { // release
			if (rq.resources <= datas[ALLOC_][rq.gid]) {
				datas[ALLOC_][rq.gid] -= rq.resources;
				avail += rq.resources;
				cout << "granted\n\nCheck waiting request:\n";
				for (auto it = begin(waiting_q); it != end(waiting_q);) {
					auto& rq = *it;
					cout << "gid " << rq.gid << ' ' << rq.op
						<< ' ' << print_all_(rq.resources) << ":\n";
					if (rq.resources <= datas[MAX_][rq.gid] - datas[ALLOC_][rq.gid]) {
						if (rq.resources <= avail) {
							avail -= rq.resources;
							datas[ALLOC_][rq.gid] += rq.resources;
							ss = is_safety_(avail, datas[ALLOC_], datas[MAX_], rq);
							if (ss.size() == gid_num) {
								cout << "granted, safe sequence = " << print_all_(ss) << '\n';
								it = waiting_q.erase(it);
							}
							else {
								avail += rq.resources;
								datas[ALLOC_][rq.gid] -= rq.resources;
								cout << "unsafe, must wait\n";
								++it;
							}
						}
						else {
							cout << "not enough resouces, must wait\n";
							++it;
						}
					}
					else {
						cout << "invalid request, not granted\n";
						it = waiting_q.erase(it);
					}
					cout << "\nAVAILABLE = " << print_all_(avail) << "\n\n";
				}
				cout << "finish checking\n";
			}
			else {
				cout << "invalid request, not granted\n";
			}
		}
		cout << "\nAVAILABLE = " << print_all_(avail) << "\n\n";
	}
}