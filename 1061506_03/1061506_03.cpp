/* 
    This cpp file is used to illustrate the invocation of pthread_create() in c++ and the compilation with g++.
    compile: g++ -o thread-ex1 thread-ex1.cpp -lpthread
    exec: ./thread-ex1 
*/
/* Includes */
#include <unistd.h>     /* Symbolic Constants */
#include <sys/types.h>  /* Primitive System Data Types */ 
#include <errno.h>      /* Errors */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <pthread.h>    /* POSIX Threads */
#include <string.h>     /* String handling */
#include <queue>
#include <string>
#include <list>

using std::string;
using std::queue;

/* prototype for thread routine */
void *student_behavior (void *ptr);
void *TA_behavior (void *ptr);
void *Prof_behavior (void *ptr);

struct my_sem_t {
   int n = 0;
   pthread_mutex_t a, b, c;
   my_sem_t() {
      pthread_mutex_init(&a, NULL);
      pthread_mutex_init(&b, NULL);
      pthread_mutex_init(&c, NULL);
      pthread_mutex_lock(&b);
   }
   ~my_sem_t() {
      pthread_mutex_destroy(&a);
      pthread_mutex_destroy(&b);
      pthread_mutex_destroy(&c);
   }
   void wait() {
      pthread_mutex_lock(&a);
      pthread_mutex_lock(&c);
      if(--n < 0) {
         pthread_mutex_unlock(&c);
         pthread_mutex_lock(&b);
      }
      else
         pthread_mutex_unlock(&c);
      pthread_mutex_unlock(&a);
   }
   void signal() {
      pthread_mutex_lock(&c);
      if(!++n)
         pthread_mutex_unlock(&b);
      pthread_mutex_unlock(&c);
   }
};

/* struct to hold data to be passed to a thread
   this shows how multiple data items can be passed to a thread */
struct student_data_t {
    int sid, TA_id = -1, dis_time;
    bool can_talk_with_TA = false, had_talked_with_TA = false;
    my_sem_t ack;
} student_datas[50];


pthread_mutex_t TA_queue_mutex, Prof_queue_mutex;
struct pt_data_t {
    int tid = 0;
    my_sem_t ack;
    string name;
} pt_datas[3];

int TA_num;
int TY_core_num;
int wait_TA_num;
queue<int> wait_TA_queue, wait_Prof_queue;
queue<int> idle_TA_queue, idle_Prof_queue;

bool end_ = false;

pthread_mutex_t mutex_;

struct timespec operator-(struct timespec end, struct timespec const& start) {
    end.tv_sec -= start.tv_sec;
    end.tv_nsec -= start.tv_nsec;
    if(end.tv_nsec < 0) {
        --end.tv_sec;
        end.tv_nsec += 1'000'000'000;
    }
    return end;
}

inline int rnd(int begin, int end) {
    return begin + rand() % (end - begin + 1);
}

// pthread_mutex_trylock

struct initializer {
    initializer() {
        srand(0);
        pthread_mutex_init(&mutex_, NULL);
        pthread_mutex_init(&TA_queue_mutex, NULL);
        pthread_mutex_init(&Prof_queue_mutex, NULL);

        /* initialize data to pass to student thread */
        for(int i = 0; i < 50; ++i)
            student_datas[i].sid = i + 1;

        char names[][20] = {
            "Prof. TY",
            "TA Y",
            "TA C",
        };
        for(int i = 0; i <= TA_num; ++i)
            pt_datas[i].name = names[i];
    }
    ~initializer() {
        pthread_mutex_destroy(&mutex_);
        pthread_mutex_destroy(&TA_queue_mutex);
        pthread_mutex_destroy(&Prof_queue_mutex);
    }
};

int main(int argc, char* argv[]) {
    if(argc != 3) {
        fprintf(stderr, "execute with: \"./TYSIM #TA_num(1~2) #enable_double_core(0 or 1)\"\n");
        exit(EXIT_FAILURE);
    } else {
        TA_num = std::stoi(argv[1]);
        if(TA_num < 1 || TA_num > 2) {
            fprintf(stderr, "The range of #TA_num is [1,2]\"\n");
            exit(EXIT_FAILURE);
        }
        TY_core_num = std::stoi(argv[2]);
        if(TY_core_num != 0 && TY_core_num != 1) {
            fprintf(stderr, "The value of #enable_double_core should be 0 or 1\"\n");
            exit(EXIT_FAILURE);
        }
        ++TY_core_num;
    }
    initializer init;
    pthread_t threads[53]{};  /* thread variables */
    
    /* create threads 1 and 2 */
    pthread_create (&threads[0], NULL,  Prof_behavior, (void *) &pt_datas[0]);
    for(int i = 0; i < TA_num; ++i) {
        pt_datas[i + 1].tid = i + 1;
        pthread_create (&threads[i + 1], NULL,  TA_behavior, (void *) &pt_datas[i + 1]);
    }
    for(int i = 0; i < 50; ++i)
        pthread_create (&threads[i + 3], NULL,  student_behavior, (void *) &student_datas[i]);

    /* Main block now waits for both threads to terminate, before it exits
       If main block exits, both threads exit, even if the threads have not
       finished their work */
    for(int i = 0; i < 53; ++i)
        if(threads[i])
            pthread_join(threads[i], NULL);
    /* exit */  
    exit(0);
} /* main() */


/**
 * print_message_function is used as the start routine for the threads used
 * it accepts a void pointer 
**/
static struct timespec start;
long long clock_now_() {
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t = t - start;
    return t.tv_sec * 1'000LL + t.tv_nsec / 1'000'000;
}

void* student_behavior(void *ptr) {
    long long msec;
    student_data_t& data = *reinterpret_cast<student_data_t*>(ptr);  /* type cast to a pointer to thdata */
    pthread_mutex_lock(&mutex_);
    usleep(rnd(5, 10) * 1'000);
    pthread_mutex_unlock(&mutex_);
    
    printf("%5lld ms -- Student %.2d: enter\n", clock_now_(), data.sid);
    /* do the work */
    while(!data.can_talk_with_TA) {
        pthread_mutex_lock(&TA_queue_mutex);
        if(wait_TA_queue.size() < 5) {
            if(idle_TA_queue.size()) {
                int TA_id = idle_TA_queue.front();
                pt_datas[TA_id].ack.signal();          
                idle_TA_queue.pop();      
            }
            data.can_talk_with_TA = true;
            printf("%5lld ms -- Student %.2d: wait TA\n", clock_now_(), data.sid);
            wait_TA_queue.push(data.sid);
            pthread_mutex_unlock(&TA_queue_mutex);
            data.ack.wait();
        }
        else {
            long long msec = rnd(30, 50);
            printf("%5lld ms -- Student %.2d: go watching \"The Distance Between Us And The Hunger\" with TA S %lld ms\n", clock_now_(), data.sid, msec);
            pthread_mutex_unlock(&TA_queue_mutex);
            usleep(msec * 1'000);
        }
    }
    usleep(data.dis_time * 1'000);
    data.had_talked_with_TA = true;
    pthread_mutex_lock(&Prof_queue_mutex);
    if(idle_Prof_queue.size()) {
        pt_datas[data.TA_id].ack.signal(); // tell leave
        pt_datas[0].ack.signal();
        printf("%5lld ms -- Student %.2d: finish the discussion with %s\n", clock_now_(), data.sid, pt_datas[data.TA_id].name.c_str());
        idle_Prof_queue.pop();
        wait_Prof_queue.push(data.sid);
        pthread_mutex_unlock(&Prof_queue_mutex);
        data.ack.wait();
    }
    else {
        pthread_mutex_unlock(&Prof_queue_mutex);
        pthread_mutex_lock(&TA_queue_mutex);
        pt_datas[data.TA_id].ack.signal(); // tell leave
        wait_TA_queue.push(data.sid);
        printf("%5lld ms -- Student %.2d: finish the discussion with %s and give up his/her seat\n", clock_now_(), data.sid, pt_datas[data.TA_id].name.c_str());
        pthread_mutex_unlock(&TA_queue_mutex);
        data.ack.wait();
        printf("%5lld ms -- Student %.2d: sit in front of %s and wait Prof. TY\n", clock_now_(), data.sid, pt_datas[data.TA_id].name.c_str());
        pthread_mutex_lock(&Prof_queue_mutex);
        wait_Prof_queue.push(data.sid);
        if(idle_Prof_queue.size()) {
            pt_datas[0].ack.signal();
            idle_Prof_queue.pop();
        }
        pthread_mutex_unlock(&Prof_queue_mutex);
        data.ack.wait();
        pt_datas[data.TA_id].ack.signal(); // tell leave
    }

    usleep(data.dis_time * 1'000);
    printf("%5lld ms -- Student %.2d: finish the discussion with %s and leave\n", clock_now_(), data.sid, pt_datas[0].name.c_str());
    pt_datas[0].ack.signal(); // tell leave
    pthread_exit(0); /* exit */
} /* print_message_function ( void *ptr ) */

decltype(auto) discuss_with_student(queue<int>& q, pthread_mutex_t& mut, int range_begin, int range_end, int who) {
    auto& sdata = student_datas[q.front() - 1];
    q.pop();
    pthread_mutex_unlock(&mut);
    sdata.dis_time = rnd(range_begin, range_end);
    if(who)
        sdata.TA_id = who;
    sdata.ack.signal();
    return sdata;
}

void* Prof_behavior(void* ptr) {
    pt_data_t& data = *reinterpret_cast<pt_data_t*>(ptr);  /* type cast to a pointer to thdata */

    clock_gettime(CLOCK_REALTIME, &start);
    for(int i = 0; i < TA_num; ++i)
        pt_datas[i + 1].ack.signal();
    pthread_mutex_lock(&Prof_queue_mutex);
    for(int i = 0; i < TY_core_num; ++i)
        idle_Prof_queue.push(0);
    pthread_mutex_unlock(&Prof_queue_mutex);
    printf("%5lld ms -- %s: rest\n", clock_now_(), data.name.c_str());

    int s_num = 0;
    while(s_num < 50) {
        data.ack.wait();
        pthread_mutex_lock(&Prof_queue_mutex);
        if(wait_Prof_queue.empty()) {
            printf("%5lld ms -- %s: rest\n", clock_now_(), data.name.c_str());
            idle_Prof_queue.push(0);
            pthread_mutex_unlock(&Prof_queue_mutex);
        }
        else {
            auto& sdata = discuss_with_student(wait_Prof_queue, Prof_queue_mutex, 50, 100, data.tid);
            printf("%5lld ms -- %s: discuss with Student %.2d %d ms\n", clock_now_(), data.name.c_str(), sdata.sid, sdata.dis_time);
            ++s_num;
        }
    }
    
    end_ = true;

    for(int i = 0; i < TA_num; ++i)
        pt_datas[i + 1].ack.signal();

    pthread_exit(0); /* exit */
}

void* TA_behavior(void* ptr) {
    pt_data_t& data = *reinterpret_cast<pt_data_t*>(ptr);  /* type cast to a pointer to thdata */
    data.ack.wait();
    pthread_mutex_lock(&TA_queue_mutex);
    idle_TA_queue.push(data.tid);
    printf("%5lld ms -- %s: rest\n", clock_now_(), data.name.c_str());
    pthread_mutex_unlock(&TA_queue_mutex);

    while(true) {
        data.ack.wait();
        if(end_)
            break;
        pthread_mutex_lock(&TA_queue_mutex);
        if(wait_TA_queue.empty()) {
            printf("%5lld ms -- %s: rest\n", clock_now_(), data.name.c_str());
            idle_TA_queue.push(data.tid);
            pthread_mutex_unlock(&TA_queue_mutex);
        }
        else {
            auto& sdata = discuss_with_student(wait_TA_queue, TA_queue_mutex, 10, 30, data.tid);
            if(!sdata.had_talked_with_TA)
                printf("%5lld ms -- %s: discuss with Student %.2d %d ms\n", clock_now_(), data.name.c_str(), sdata.sid, sdata.dis_time);
        }
    }

    pthread_exit(0); /* exit */
}