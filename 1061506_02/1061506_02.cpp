/* 
    This cpp file is used to illustrate the invocation of pthread_create() in c++ and the compilation with g++.
    compile: g++ -o prog2_1506 1061506_02.cpp -std=c++17 -lpthread
    exec: ./prog2_1506 filename 
*/
/* Includes */
#include <algorithm>    // all_of, replace_if, max_element
#include <errno.h>      /* Errors */
#include <fstream>
#include <functional>
#include <math.h>
#include <map>
#include <numeric>
#include <pthread.h>    /* POSIX Threads */
#include <sstream>
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <string>
#include <sys/types.h>  /* Primitive System Data Types */ 
#include <unistd.h>     /* Symbolic Constants */
#include <vector>
using namespace std;

/* prototype for thread routine */
void* article_analyzer(void *ptr);

/* struct to hold data to be passed to a thread
   this shows how multiple data items can be passed to a thread */
typedef struct str_thdata
{
    int thread_no;  // 幾號thread的資料就存在vector中的哪一個位置
    string doc_id;  // 文件id
    string article; // 文件內容
    double avg_cosine = 0.0; // 文件的平均相似度

    bool operator<(const str_thdata& rhs) const {
        return rhs.avg_cosine - avg_cosine > 1e-9 || abs(avg_cosine - rhs.avg_cosine) <= 1e-9 && doc_id > rhs.doc_id;
    }
} thdata;

int Num, Update_Num;
map<string, int> Basis;             // 用來紀錄所有有出現的字
vector<map<string, int> > Doc_Vecs; // 所有文件各自的詞頻向量
vector<double> Vec_Lens;            // 紀錄每份文件各自的向量長度 方便計算cosine值
vector<thdata> Datas;         /* structs to be passed to threads */
vector<pthread_t> Threads;  /* thread variables */
pthread_mutex_t all_fin_mutex, all_updated_mutex, update_vec_mutex, output_mutex; // 保持同步的mutex

void init(string file_name) {
    all_fin_mutex = PTHREAD_MUTEX_INITIALIZER;
    all_updated_mutex = PTHREAD_MUTEX_INITIALIZER;
    update_vec_mutex = PTHREAD_MUTEX_INITIALIZER;
    output_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&all_fin_mutex);
    pthread_mutex_lock(&all_updated_mutex);

    thdata td;
    ifstream fin(file_name);
    while(getline(fin, td.doc_id)) {
        getline(fin, td.article);
        Datas.push_back(td);
        ++Num;
    }
    Doc_Vecs.resize(Num);
    Vec_Lens.resize(Num);
}

void create_threads() {
    Threads.resize(Num); // 每一個文件都需要一個thread處理 共需要num個thread
    int count = 0;
    for(auto& data : Datas) {
        data.thread_no = count;
        pthread_create(&Threads[count], NULL, article_analyzer, (void *) &data);
        printf("[Main thread]: create TID:%lu, DocID:%s\n", Threads[count++], data.doc_id.c_str());
    }
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        fprintf(stderr, "fatal error: no input file or too many input files\n");
        exit(EXIT_FAILURE);
    }
    
    init(argv[1]);

    create_threads();

    for(auto& thr : Threads)     // 等待所有thread完成任務
        pthread_join(thr, NULL);
    
    // 尋找關鍵文件
    auto& key_doc = *max_element(Datas.begin(), Datas.end());
    printf("[Main thread] KeyDocID:%s Highest Average Cosine: %.6lf\n", key_doc.doc_id.c_str(), key_doc.avg_cosine);
    // 計算Main thread的CPU time
    timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    printf("[Main thread] CPU time: %ldms\n", t.tv_nsec / 1000000);
    /* exit */  
    exit(0);
} /* main() */

void* article_analyzer ( void *ptr )
{
    thdata& data = *(thdata *) ptr;  /* type cast to a pointer to thdata */
    pthread_t tid = pthread_self(); // 取得自己的tid
    map<string, int>& vec = Doc_Vecs[data.thread_no]; // 自己的詞頻向量表

    replace_if(data.article.begin(), data.article.end(), [](char c)->bool { return ispunct(c); }, ' '); // 文件內容只要是符號就替換成空白
    stringstream ss(data.article);
    string tmp;
    while(ss >> tmp)
        if(all_of(tmp.begin(), tmp.end(), [](char c)->bool { return isalpha(c); }))
            ++vec[tmp];

    // 現在要統一有出現過哪些字
    pthread_mutex_lock(&update_vec_mutex); // 等待上一個人更新完
    for(auto& [word, count] : vec)  // 將自己出現過的所有字
        Basis[word];                // 都加到Basis裡面
    if(++Update_Num == Num)
        pthread_mutex_unlock(&all_updated_mutex);
    pthread_mutex_unlock(&update_vec_mutex);

    pthread_mutex_lock(&all_updated_mutex); // 等待所有人都把資料更新上去
    pthread_mutex_unlock(&all_updated_mutex);
    for(auto& [word, count] : Basis)    // 看看Basis裡面整理的所有字
        vec[word];                      // 自己有出現過則出現次數不變 沒有則新增並歸零出現次數
    pthread_mutex_lock(&output_mutex);
    printf("[TID=%lu] DocID:%s [", tid, data.doc_id.c_str());
    int not_first = 0;
    Vec_Lens[data.thread_no] = 0.0;
    for(auto& [word, count] : vec) {
        Vec_Lens[data.thread_no] += count * count;
        if(not_first++)
            printf(",");
        printf("%d", count);
    }
    Vec_Lens[data.thread_no] = sqrt(Vec_Lens[data.thread_no]);
    puts("]");
    if(!--Update_Num)
        pthread_mutex_unlock(&all_fin_mutex);
    pthread_mutex_unlock(&output_mutex);

    pthread_mutex_lock(&all_fin_mutex);
    pthread_mutex_unlock(&all_fin_mutex);
    for(int i = 0; i < Num; ++i) {
        if(i != data.thread_no) {
            double cos = 0.0;
            for(auto it1 = vec.begin(), it2 = Doc_Vecs[i].begin(); it1 != vec.end(); ++it1, ++it2)
                cos += it1->second * it2->second;
            cos /= Vec_Lens[data.thread_no] * Vec_Lens[i];
            data.avg_cosine += cos / (Num - 1);
            printf("[TID=%lu] cosine(%s,%s)=%.6lf\n", tid, data.doc_id.c_str(), Datas[i].doc_id.c_str(), cos);
        }
    }
    printf("[TID=%lu] Avg_cosine: %.6lf\n", tid, data.avg_cosine);
    timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    printf("[TID=%lu] CPU time: %ldms\n", tid, t.tv_nsec / 1000000);
    pthread_exit(0); /* exit */
} /* article_analyzer ( void *ptr ) */