#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <list>
#include <algorithm>
#include <iomanip>

using namespace std;

class Trace{
	public:
		Trace();
		Trace(string type_str, string addr_str);
		unsigned short type;
		unsigned int addr;
		unsigned int get_index(int block_sz, int nset);
		unsigned int get_tag(int block_sz, int nset);
};

Trace::Trace(){
	type = 0;
	addr = 0;
}

Trace::Trace(string type_str, string addr_str){
	type = (unsigned short) stoi(type_str);
	addr = (unsigned int) strtoul(addr_str.c_str(), NULL, 16);	
}

/* get index of given address */
unsigned int Trace::get_index(int block_sz, int nset){
	
	unsigned int index = addr;
	index >>= (int) log2(block_sz);
	index %= nset;

	return index;

}

/* get tag bit of given address */
unsigned int Trace::get_tag(int block_sz, int nset){

	unsigned int tag = addr;
	tag >>= (int) log2(block_sz);
	tag >>= (int) log2(nset);

	return tag;

}

class CacheBlock{
	public:
		CacheBlock();
		unsigned int tag;
		bool dirty;
		bool operator == (const CacheBlock& block);
};

CacheBlock::CacheBlock(){
	tag = 0;
	dirty = false;
}

bool CacheBlock:: operator == (const CacheBlock& block){
	return tag == block.tag;
}

vector<Trace> traces_1;
vector<Trace> traces_2;

/* miss ratio table for L1 I-cache and D-cache */
double L1_I_miss_16[4][5];
double L1_I_miss_64[4][5];
double L1_D_miss_16[4][5];
double L1_D_miss_64[4][5];

/* miss ratio table for L2 cache inclusive and exclusive */
double L2_in_miss_16[4][5];
double L2_in_miss_64[4][5];
double L2_ex_miss_16[4][5];
double L2_ex_miss_64[4][5];

/* memory block writes table for D-cache */
int D_mem_in_16[4][5];
int D_mem_in_64[4][5];
int D_mem_ex_16[4][5];
int D_mem_ex_64[4][5];

/* given size as const */
const int CACHE_SZ[5] = {1024, 2048, 4096, 8192, 16384};
const int BLOCK_SZ[2] = {16, 64};
const int ASSOC[4] = {1, 2, 4, 8};

/* this follows inclusive policy */
void cache_simulate_in(int cache_sz, int block_sz, int assoc){
	
	int L1_nset = cache_sz / block_sz / assoc;
	int L2_nset = 16384 / block_sz / 8;
	const size_t L1_MAX_SZ = assoc;
	const size_t L2_MAX_SZ = 8;

	list<CacheBlock> L1_I[L1_nset];
	list<CacheBlock> L1_D[L1_nset];
	list<CacheBlock> L2[L2_nset];

	int L1_I_try = 0;
	int L1_I_miss = 0;
	int L1_D_try = 0;
	int L1_D_miss = 0;

	int L2_try = 0;
	int L2_miss = 0;
	int memory_write = 0;

	for(size_t k = 0; k < traces_1.size(); k++){

		Trace t = traces_1[k];
		CacheBlock block; // cache block for L1
		CacheBlock block_L2; // cache block for L2
		unsigned int i = t.get_index(block_sz, L1_nset);
		unsigned int i_L2 = t.get_index(block_sz, L2_nset); // cache index for L2

		if(t.type == 0){ // read data (L1_D)

			L1_D_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_D[i].begin(), L1_D[i].end(), block);

			if(it == L1_D[i].end()){ // not found (L1_D miss)

				L1_D_miss++;
				if(L1_D[i].size() == L1_MAX_SZ){ // set is full
					L1_D[i].pop_front(); // block evicted from L1 cache
				}
				L1_D[i].push_back(block); // add in L1 cache
				
				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);
				
				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++;
					if(L2[i_L2].size() == L2_MAX_SZ){ // set is full
						
						unsigned int L1_addr = L2[i_L2].front().tag; // reconstruct address in L2 block
						bool is_dirty = L2[i_L2].front().dirty;

						if(is_dirty)
							memory_write++;

						L1_addr <<= i_L2;
						L1_addr += i_L2;
						L1_addr <<= block_sz;
						
						Trace target;
						CacheBlock target_block;
						target.addr = L1_addr;
						unsigned int target_i = target.get_index(block_sz, L1_nset);
						target_block.tag = target.get_tag(block_sz, L1_nset);

						list<CacheBlock>::iterator target_it = find(L1_D[target_i].begin(), L1_D[target_i].end(), target_block);					
						if(target_it != L1_D[target_i].end()){
							L1_D[target_i].erase(target_it); // same block in L1 also evicted (if in L1 D-cache)
						}
						target_it = find(L1_I[target_i].begin(), L1_I[target_i].end(), target_block);
						if(target_it != L1_I[target_i].end()){
							L1_I[target_i].erase(target_it); // same block in L1 also evicted (if in L1 I-cache)
						}

						L2[i_L2].pop_front(); // block evicted from L2 cache
					
					}
					L2[i_L2].push_back(block_L2);
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2);
					L2[i_L2].push_back(block_L2);
				}

			}
			else{ // found (L1_D hit)
				L1_D[i].erase(it);
				L1_D[i].push_back(block);
			}

		}
		else if(t.type == 1){ // write data (L1_D)
			
			L1_D_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_D[i].begin(), L1_D[i].end(), block);
			
			if(it == L1_D[i].end()){ // not found (L1_D miss)

				L1_D_miss++;
				if(L1_D[i].size() == L1_MAX_SZ){ // set is full
					L1_D[i].pop_front(); // delete by LRU policy
				}
				L1_D[i].push_back(block);

				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);

				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++;
					if(L2[i_L2].size() == L2_MAX_SZ){ // set is full

						unsigned int L1_addr = L2[i_L2].front().tag; // reconstruct address in L2 block
						bool is_dirty = L2[i_L2].front().dirty;

						if(is_dirty)
							memory_write++;

						L1_addr <<= i_L2;
						L1_addr += i_L2;
						L1_addr <<= block_sz;

						Trace target;
						CacheBlock target_block;
						target.addr = L1_addr;
						unsigned int target_i = target.get_index(block_sz, L1_nset);
						target_block.tag = target.get_tag(block_sz, L1_nset);

						list<CacheBlock>::iterator target_it = find(L1_D[target_i].begin(), L1_D[target_i].end(), target_block);					
						if(target_it != L1_D[target_i].end()){
							L1_D[target_i].erase(target_it); // same block in L1 also evicted (if in L1 D-cache)
						}
						target_it = find(L1_I[target_i].begin(), L1_I[target_i].end(), target_block);
						if(target_it != L1_I[target_i].end()){
							L1_I[target_i].erase(target_it); // same block in L1 also evicted (if in L1 I-cache)
						}

						L2[i_L2].pop_front(); // block evicted from L2 cache

					}
					L2[i_L2].push_back(block_L2);
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2);
					block_L2.dirty = true; // write L2 hit -> becomes dirty
					L2[i_L2].push_back(block_L2);
				}

			}
			else{ // found (L1_D hit)
				L1_D[i].erase(it);
				block.dirty = true; // write L1_D hit -> becomes dirty
				L1_D[i].push_back(block);
			}
		}
		else if(t.type == 2){ // instruction fetch (L1_I)
			L1_I_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_I[i].begin(), L1_I[i].end(), block);
			if(it == L1_I[i].end()){ // not found (L1_I miss)
				L1_I_miss++;
				if(L1_I[i].size() == L1_MAX_SZ) // set is full
					L1_I[i].pop_front(); // delete by LRU policy
				L1_I[i].push_back(block);

				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);

				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++;
					if(L2[i_L2].size() == L2_MAX_SZ){ // set is full

						unsigned int L1_addr = L2[i_L2].front().tag; // reconstruct address in L2 block
						L1_addr <<= i_L2;
						L1_addr += i_L2;
						L1_addr <<= block_sz;

						Trace target;
						CacheBlock target_block;
						target.addr = L1_addr;
						unsigned int target_i = target.get_index(block_sz, L1_nset);
						target_block.tag = target.get_tag(block_sz, L1_nset);

						list<CacheBlock>::iterator target_it = find(L1_D[target_i].begin(), L1_D[target_i].end(), target_block);					
						if(target_it != L1_D[target_i].end()){
							L1_D[target_i].erase(target_it); // same block in L1 also evicted (if in L1 D-cache)
						}
						target_it = find(L1_I[target_i].begin(), L1_I[target_i].end(), target_block);
						if(target_it != L1_I[target_i].end()){
							L1_I[target_i].erase(target_it); // same block in L1 also evicted (if in L1 I-cache)
						}

						L2[i_L2].pop_front(); // block evicted from L2 cache

					}
					L2[i_L2].push_back(block_L2);
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2);
					block_L2.dirty = true; // write L2 hit -> becomes dirty
					L2[i_L2].push_back(block_L2);
				}

			}
			else{ // found (L1_I hit)
				L1_I[i].erase(it);
				L1_I[i].push_back(block);
			}
		}
		else{
			cout << "Invalid type of input" << endl;
			exit(1);
		}

	}

	int row = (int) log2(assoc);
	int col = (int) log2(cache_sz) - 10;
	double i_miss = (double) L1_I_miss / L1_I_try;
	double d_miss = (double) L1_D_miss / L1_D_try;
	double l2_miss = (double) L2_miss / L2_try;

	if(block_sz == 16){
		L1_I_miss_16[row][col] = i_miss;
		L1_D_miss_16[row][col] = d_miss;
		L2_in_miss_16[row][col] = l2_miss;
		D_mem_in_16[row][col] = memory_write;
	}
	else if(block_sz == 64){
		L1_I_miss_64[row][col] = i_miss;
		L1_D_miss_64[row][col] = d_miss;
		L2_in_miss_64[row][col] = l2_miss;
		D_mem_in_64[row][col] = memory_write;
	}

	cout.setf(ios::fixed);
	cout.precision(4);

}

/* this follows exclusive policy */
void cache_simulate_ex(int cache_sz, int block_sz, int assoc){

	int L1_nset = cache_sz / block_sz / assoc;
	int L2_nset = 16384 / block_sz / 8;
	const size_t L1_MAX_SZ = assoc;
	const size_t L2_MAX_SZ = 8;

	list<CacheBlock> L1_I[L1_nset];
	list<CacheBlock> L1_D[L1_nset];
	list<CacheBlock> L2[L2_nset];

	int L1_I_try = 0;
	int L1_I_miss = 0;
	int L1_D_try = 0;
	int L1_D_miss = 0;

	int L2_try = 0;
	int L2_miss = 0;
	int memory_write = 0;

	for(size_t k = 0; k < traces_1.size(); k++){

		Trace t = traces_1[k];
		CacheBlock block; // cache block for L1
		CacheBlock block_L2; // cache block for L2
		unsigned int i = t.get_index(block_sz, L1_nset);
		unsigned int i_L2 = t.get_index(block_sz, L2_nset); // cache index for L2

		if(t.type == 0){ // read data (L1_D)

			L1_D_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_D[i].begin(), L1_D[i].end(), block);

			if(it == L1_D[i].end()){ // not found (L1_D miss)

				L1_D_miss++;
				if(L1_D[i].size() == L1_MAX_SZ){ // set is full

					unsigned int L2_addr = L1_D[i].front().tag; // reconstruct address in L1 block
					L2_addr <<= i;
					L2_addr += i;
					L2_addr <<= block_sz;

					Trace target;
					CacheBlock target_block;
					target.addr = L2_addr;
					unsigned int target_i = target.get_index(block_sz, L2_nset);
					target_block.tag = target.get_tag(block_sz, L2_nset);
					target_block.dirty = L1_D[i].front().dirty; // set dirty bit same as L1

					if(L2[target_i].size() == L2_MAX_SZ){ // L2 set is full
						bool is_dirty = L2[target_i].front().dirty;
						if(is_dirty)
							memory_write++; // if evicted block in L2 cache is dirty -> memory write
						L2[target_i].pop_front();
					}
					L2[target_i].push_back(target_block); // add L1 evicted block to L2 cache

					L1_D[i].pop_front(); // block evicted from L1 cache
				
				}
				L1_D[i].push_back(block); // add in L1 cache

				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);

				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++;
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2); // remove block from L2
				}

			}
			else{ // found (L1_D hit)
				L1_D[i].erase(it);
				L1_D[i].push_back(block);
			}

		}
		else if(t.type == 1){ // write data (L1_D)

			L1_D_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_D[i].begin(), L1_D[i].end(), block);

			if(it == L1_D[i].end()){ // not found (L1_D miss)

				L1_D_miss++;
				if(L1_D[i].size() == L1_MAX_SZ){ // set is full

					unsigned int L2_addr = L1_D[i].front().tag; // reconstruct address in L1 block
					L2_addr <<= i;
					L2_addr += i;
					L2_addr <<= block_sz;

					Trace target;
					CacheBlock target_block;
					target.addr = L2_addr;
					unsigned int target_i = target.get_index(block_sz, L2_nset);
					target_block.tag = target.get_tag(block_sz, L2_nset);
					target_block.dirty = L1_D[i].front().dirty; // set dirty bit same as L1

					if(L2[target_i].size() == L2_MAX_SZ){ // L2 set is full
						bool is_dirty = L2[target_i].front().dirty;
						if(is_dirty)
							memory_write++; // if evicted block in L2 cache is dirty -> memory write
						L2[target_i].pop_front();
					}
					L2[target_i].push_back(target_block); // add L1 evicted block to L2 cache

					L1_D[i].pop_front(); // block evicted from L1 cache

				}
				L1_D[i].push_back(block);

				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);

				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++; // only add block to L1 cache
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2); // remove block from L2
				}

			}
			else{ // found (L1_D hit)
				L1_D[i].erase(it);
				block.dirty = true; // write L1_D hit -> becomes dirty
				L1_D[i].push_back(block);
			}
		}
		else if(t.type == 2){ // instruction fetch (L1_I)
			L1_I_try++;
			block.tag = t.get_tag(block_sz, L1_nset);
			list<CacheBlock>::iterator it = find(L1_I[i].begin(), L1_I[i].end(), block);
			if(it == L1_I[i].end()){ // not found (L1_I miss)
				L1_I_miss++;
				if(L1_I[i].size() == L1_MAX_SZ){ // set is full
					unsigned int L2_addr = L1_I[i].front().tag; // reconstruct address in L1 block
					L2_addr <<= i;
					L2_addr += i;
					L2_addr <<= block_sz;

					Trace target;
					CacheBlock target_block;
					target.addr = L2_addr;
					unsigned int target_i = target.get_index(block_sz, L2_nset);
					target_block.tag = target.get_tag(block_sz, L2_nset);
					target_block.dirty = L1_I[i].front().dirty; // set dirty bit same as L1

					if(L2[target_i].size() == L2_MAX_SZ){ // L2 set is full
						bool is_dirty = L2[target_i].front().dirty;
						if(is_dirty)
							memory_write++; // if evicted block in L2 cache is dirty -> memory write
						L2[target_i].pop_front();
					}
					L2[target_i].push_back(target_block); // add L1 evicted block to L2 cache

					L1_I[i].pop_front(); // block evicted from L1 cache
				}
				L1_I[i].push_back(block);

				L2_try++; // try finding block in L2 cache
				block_L2.tag = t.get_tag(block_sz, L2_nset);
				list<CacheBlock>::iterator it_L2 = find(L2[i_L2].begin(), L2[i_L2].end(), block_L2);

				if(it_L2 == L2[i_L2].end()){ // not found (L1 miss, L2 miss)
					L2_miss++;
					if(L2[i_L2].size() == L2_MAX_SZ){ // set is full

						unsigned int L1_addr = L2[i_L2].front().tag; // reconstruct address in L2 block
						L1_addr <<= i_L2;
						L1_addr += i_L2;
						L1_addr <<= block_sz;

						Trace target;
						CacheBlock target_block;
						target.addr = L1_addr;
						unsigned int target_i = target.get_index(block_sz, L1_nset);
						target_block.tag = target.get_tag(block_sz, L1_nset);

						list<CacheBlock>::iterator target_it = find(L1_D[target_i].begin(), L1_D[target_i].end(), target_block);					
						if(target_it != L1_D[target_i].end()){
							L1_D[target_i].erase(target_it); // same block in L1 also evicted (if in L1 D-cache)
						}
						target_it = find(L1_I[target_i].begin(), L1_I[target_i].end(), target_block);
						if(target_it != L1_I[target_i].end()){
							L1_I[target_i].erase(target_it); // same block in L1 also evicted (if in L1 I-cache)
						}

						L2[i_L2].pop_front(); // block evicted from L2 cache

					}
					L2[i_L2].push_back(block_L2);
				}
				else{ // found (L1 miss, L2 hit)
					L2[i_L2].erase(it_L2); // remove block from L2
				}

			}
			else{ // found (L1_I hit)
				L1_I[i].erase(it);
				L1_I[i].push_back(block);
			}
		}
		else{
			cout << "Invalid type of input" << endl;
			exit(1);
		}

	}

	int row = (int) log2(assoc);
	int col = (int) log2(cache_sz) - 10;
	double l2_miss = (double) L2_miss / L2_try;

	if(block_sz == 16){
		L2_ex_miss_16[row][col] = l2_miss;
		D_mem_ex_16[row][col] = memory_write;
	}
	else if(block_sz == 64){
		L2_ex_miss_64[row][col] = l2_miss;
		D_mem_ex_64[row][col] = memory_write;
	}

	cout.setf(ios::fixed);
	cout.precision(4);

}

void cache_proc(void){

	for(int i : CACHE_SZ){
		for(int j : BLOCK_SZ){
			for(int k : ASSOC){
				cache_simulate_in(i, j, k);
				cache_simulate_ex(i, j, k);
			}
		}
	}

	cout << "<L1 I-cache Miss Ratio (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L1_I_miss_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L1 I-cache Miss Ratio (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L1_I_miss_64[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L1 D-cache Miss Ratio (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L1_D_miss_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L1 D-cache Miss Ratio (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L1_D_miss_64[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L2 Inclusive Miss Ratio (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L2_in_miss_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L2 Inclusive Miss Ratio (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L2_in_miss_64[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L2 Exclusive Miss Ratio (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L2_ex_miss_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<L2 Exclusive Miss Ratio (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << L2_ex_miss_64[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<Inclusive Memory Block Writes (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << D_mem_in_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<Inclusive Memory Block Writes (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << D_mem_in_64[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<Exclusive Memory Block Writes (block size = 16B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << D_mem_ex_16[i][j];
		cout << endl;
	}
	cout << endl;
	cout << "<Exclusive Memory Block Writes (block size = 64B)>" << endl;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 5; j++)
			cout << setw(8) << D_mem_ex_64[i][j];
		cout << endl;
	}

}

int main(int argc, char* argv[]){

	ifstream fread_1(argv[1]); // open file
	string line;
	stringstream ss;

	/* read file and push each entry into vector */
	while(getline(fread_1, line)){	

		ss.clear();
		ss << line;
		string type, addr;
		ss >> type >> addr;

		Trace trace(type, addr);
		traces_1.push_back(trace);

	}

	cache_proc();

}
