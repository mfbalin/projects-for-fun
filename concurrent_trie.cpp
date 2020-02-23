#include <chrono>
#include <string>
#include <mutex>
#include <iostream>

class timer {
private:
	const std::string name;
	const std::chrono::high_resolution_clock::time_point start;
	std::mutex &mtx;
	std::ostream &os;
public:
	static std::mutex iomutex;
	timer(std::string s, std::mutex &_mtx = iomutex, std::ostream &_os = std::cerr) : name(s), start(std::chrono::high_resolution_clock::now()), mtx(_mtx), os(_os) {
		std::lock_guard<std::mutex> lock(mtx);
		os << name << " has started" << std::endl;
	}
	~timer() {
		std::lock_guard<std::mutex> lock(mtx);
		os << name << " took " << time() << "s" << std::endl;
	}
	double time() const {
		return std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start).count();
	}
};

std::mutex timer::iomutex;

// #include <cuda/std/std/atomic>

#include <memory>
#include <thread>
#include <string>
#include <streambuf>
#include <vector>
#include <atomic>
#include <cstdio>

struct trie {
	struct ref {
		std::atomic<trie *> ptr{nullptr};
	} next[26];
	std::atomic<std::size_t> count{0};
};

int index_of(char c) {
	if(c >= 'a' && c <= 'z') return c - 'a';
	if(c >= 'A' && c <= 'Z') return c - 'A';
	return -1;
}

auto get_ptr(const char *begin, const char *end, unsigned rank, unsigned size) {
	auto ptr = begin;
	std::advance(ptr, rank * std::distance(begin, end) / size);
	while(ptr != begin && ptr != end && index_of(*ptr) != -1)
		ptr++;
	if(ptr != end && index_of(*ptr) == -1)
		ptr++;
	return ptr;
}

void make_trie(trie &root, std::atomic<trie *> &bump, char *global_begin, char *global_end, unsigned rank, unsigned size) {
	auto begin = get_ptr(global_begin, global_end, rank, size);
	auto end = get_ptr(global_begin, global_end, rank + 1, size);
	/*{
		std::lock_guard lock(timer::iomutex);
		std::cerr << rank << "<<<\n" << std::string(begin, end) << "\n<<<\n";
	}*/
	auto local_bump = bump.fetch_add(1, std::memory_order_relaxed);
	auto n = &root;
	for(auto pc = begin; pc != end; pc++) {
		auto idx = index_of(*pc);
		if(idx == -1) {
			if(n != &root) {
				n->count.fetch_add(1, std::memory_order_relaxed);
				n = &root;
			}
			continue;
		}
		trie *next = n->next[idx].ptr.load(std::memory_order_acquire);
		if(next == nullptr && n->next[idx].ptr.compare_exchange_strong(next, local_bump, std::memory_order_release, std::memory_order_acquire)) {
			n = local_bump;
			local_bump = bump.fetch_add(1, std::memory_order_relaxed);
		}
		else
			n = next;
	}
}

void word_counts(trie &root, std::string &word, std::vector<std::pair<std::string, std::size_t>> &word_cnts) {
	auto cnt = root.count.load(std::memory_order_relaxed);
	if(cnt)
		word_cnts.emplace_back(word, cnt);
	for(char i = 0; i < 26; i++) {
		auto n = root.next[i].ptr.load(std::memory_order_relaxed);
		if(n) {
			word.push_back(i + 'a');
			word_counts(*n, word, word_cnts);
			word.pop_back();
		}
	}
}

auto word_counts(trie &root) {
	std::string word;
	std::vector<std::pair<std::string, std::size_t>> word_cnts;
	word_counts(root, word, word_cnts);
	return word_cnts;
}


// __global__
// void foo(cuda::std::atomic<int>* atomic){
//     printf("thread %d from block %d, value %d\n", threadIdx.x, blockIdx.x, (*atomic)++);
// }

int main(int argc, char *argv[]) {

	// cuda::std::atomic<int>* atomic;
	// cudaMallocManaged(&atomic, sizeof(cuda::std::atomic<int>));

	// *atomic = 0;

	// foo<<<2,32>>>(atomic);

	// cudaDeviceSynchronize();

	// cudaFree(atomic);

	const auto num_threads = argc < 2 ? std::thread::hardware_concurrency() : std::stoi(argv[1]);

	std::ios_base::sync_with_stdio(false);
	std::string s{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};//{"fatih balin"};

	std::vector<trie> memory_pool(1 << 24);
	std::atomic<trie *> bump{memory_pool.data()};

	trie root;
	{
		timer t(std::to_string(num_threads) + " threaded trie construction of a text of length " + std::to_string(s.size()));

		std::vector<std::thread> threads;
		for(unsigned i = 0; i < num_threads; i++)
			threads.emplace_back(make_trie, std::ref(root), std::ref(bump), s.data(), s.data() + s.size(), i, num_threads);

		for(auto &t: threads)
			t.join();
	}

	for(const auto &[word, cnt]: word_counts(root))
		std::cout << word << ' ' << cnt << '\n';
	return 0;
}
