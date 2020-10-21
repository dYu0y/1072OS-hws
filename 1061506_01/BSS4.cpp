/*
   compile: g++ -o BSS BSS4.cpp -lrt
   exec: ./BSS
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>

void error_and_die(const char *msg) {
   perror(msg);
   exit(EXIT_FAILURE);
}

const int MAX_P = 100;

template<typename T>
struct SHM_ {
   std::string memname_;
   size_t region_size;
   pid_t pid;
   void* ptr;

   SHM_(const char *memname) : memname_(memname), region_size(sizeof(T)), pid(getpid()) {
      int fd = shm_open(memname_.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
      if (fd == -1)
         error_and_die("shm_open");

      if (ftruncate(fd, region_size))
         error_and_die("ftruncate");

      ptr = mmap(0, region_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (ptr == MAP_FAILED)
         error_and_die("mmap");
      close(fd);
      
      new(ptr) T;
   }

   ~SHM_() {
      if(pid == getpid()) {
         if (munmap(ptr, region_size))
            error_and_die("munmap");
         
         if (shm_unlink(memname_.c_str()))
            error_and_die("shm_unlink");
      }
   }

   T* operator->() {
      return reinterpret_cast<T*>(ptr);
   }
};

struct battleship {
   bool hit = false;
   bool ready = false;
   bool sink = false;
   bool lose = false;
   size_t bombs_num = 0;
   size_t hit_num = 0;

   battleship() = default;
   ~battleship() = default;
};

struct bs_region {
   size_t player_num = 0;
   int game_state = 0;
   
   int turn = -1;
   bool ask = false;
   std::pair<int, int> hit_pos;
   battleship ships[MAX_P];
   int rest;

   int winner_id = -1;
   
   pid_t pids[MAX_P];
   std::pair<int, int> score[MAX_P];
};

const int COL = 4, ROW = 4;

std::vector<std::pair<int, int> > my_pos, attack_stack, dir{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
std::string name;

template<typename T>
void creat_n_battleship(T& bs_ptr, pid_t& id) {
   int n = bs_ptr->player_num;
   for(int i = 0; i < n - 1; ++i) {
      id = i;
      pid_t pid = fork();
      if(pid < 0) {
         bs_ptr->game_state = -1;
         break;
      }
      else if(pid > 0) {
         bs_ptr->pids[id] = getpid();
         break;
      }
      else if(i == n - 2)
         bs_ptr->pids[++id] = getpid();
   }
}

template<typename T>
void init_game(T& bs_ptr, pid_t& id) {
   for(int i = 0; i < ROW; ++i)
      for(int j = 0; j < COL; ++j)
         attack_stack.push_back(std::make_pair(i, j));
   bs_ptr->rest = bs_ptr->player_num;

   creat_n_battleship(bs_ptr, id);
}

void init_self(pid_t id) {
   pid_t pid = getpid();
   srand(time(0) + pid % 10000 * 5);
   std::random_shuffle(attack_stack.begin(), attack_stack.end());
   std::random_shuffle(begin(dir), end(dir));
   my_pos.push_back(std::make_pair(rand() % ROW, rand() % COL));
   std::pair<int, int> next;
   for(auto const& d : dir) {
      next = std::make_pair(my_pos[0].first + d.first, my_pos[0].second + d.second);
      if(next.first >= 0 && next.first < ROW && next.second >= 0 && next.second < COL)
         break;
   }
   my_pos.push_back(next);

   name += '[' + std::to_string(pid) + ' ' + (id ? "Child" : "Parent") + ']';
}

int main(int argc, char *argv[]) {
   if(argc != 2) {
      char msg[100]{};
      sprintf(msg, "run this program with \"./BSS #num\", and #num should be less than %d", MAX_P - 2);
      error_and_die(msg);
   }

   size_t player_num = atoi(argv[1]) + 2;
   if(player_num > MAX_P) {
      char msg[50]{};
      sprintf(msg, "#num should be less than %d", MAX_P - 2);
      error_and_die(msg);
   }

   int id = -1;
   SHM_<bs_region> bs_ptr("BSS");
   bs_ptr->player_num = player_num;
   
   init_game(bs_ptr, id);
   init_self(id);
   bs_ptr->score[id].second = id;
   
   char poses[50]{};
   for(auto& p : my_pos)
      sprintf(poses + strlen(poses), "(%d,%d)", p.first, p.second);
   printf("%s: The gunboat: %s\n", name.c_str(), poses);
   
   auto& ships = bs_ptr->ships;
   auto& self = ships[id];
   self.ready = true;

   while(bs_ptr->game_state == 0)
      if(!id && std::all_of(ships, ships + player_num, [](auto const& s) -> bool { return s.ready; })) {
         std::for_each(ships, ships + player_num, [](auto& s) -> void { s.ready = false; });
         bs_ptr->turn = 0;
         bs_ptr->game_state = 1;
      }

   if(bs_ptr->game_state == -1)
      error_and_die(id ? "" : "Fork Failed");
   
   while(!my_pos.empty()) {
      if(bs_ptr->turn == id && !self.ready) {
         bs_ptr->hit_pos = attack_stack.back();
         attack_stack.pop_back();
         ++self.bombs_num;
         std::for_each(ships, ships + player_num, [](auto& s) -> void { if(s.lose)  s.ready = false; });
         while(std::any_of(ships, ships + player_num, [](auto const& s) -> bool { return s.ready; }));

         printf("%s: bombing (%d,%d)\n", name.c_str(), bs_ptr->hit_pos.first, bs_ptr->hit_pos.second);
         bs_ptr->ask = true;
         self.ready = true;
         std::for_each(ships, ships + player_num, [](auto& s) -> void { if(s.lose)  s.ready = true; });
         
         while(std::any_of(ships, ships + player_num, [](auto const& s) -> bool { return !s.ready; }));
         std::for_each(ships, ships + player_num,
            [&](auto& s) -> void {
               if(s.hit) {
                  bs_ptr->score[id].first = ++self.hit_num;
                  s.hit = false;
               }
               if(!s.lose && s.sink) {
                  --bs_ptr->rest;
                  s.lose = true;
               }
            });
         if(bs_ptr->rest == 1) {
            bs_ptr->winner_id = id;
            bs_ptr->ask = false;
            break;
         }
         do
            bs_ptr->turn = (bs_ptr->turn + 1) % player_num;
         while(ships[bs_ptr->turn].lose);
         bs_ptr->ask = false;
      }
      else if(bs_ptr->ask && !self.ready) {
         char sta[30]{};
         auto it = find(my_pos.begin(), my_pos.end(), bs_ptr->hit_pos);
         if(my_pos.end() != it) {
            strcat(sta, "hit");
            my_pos.erase(it);
            self.hit = true;
            if(my_pos.empty()) {
               sprintf(sta + strlen(sta), " and sinking");
               self.sink = true;
            }
         }
         else
            strcat(sta, "missed");
         
         printf("%s: %s\n", name.c_str(), sta);
         self.ready = true;
         while(bs_ptr->ask);
      }
      self.ready = false;
   }

   if(id < player_num - 1) {
      int status;
      waitpid(bs_ptr->pids[id + 1], &status, 0);
   }

   if (id)   exit(0);

   std::sort(bs_ptr->score, bs_ptr->score + player_num, std::greater<std::pair<int, int> >());
   puts("");
   for(int i = 0; i < std::min(player_num, 5ul); ++i)
      printf("%s: %d makes %d hits!!\n", name.c_str(), bs_ptr->pids[bs_ptr->score[i].second], bs_ptr->score[i].first);
   puts("");
   printf("%s: %d wins with %lu bomb%s\n", name.c_str(), bs_ptr->pids[bs_ptr->winner_id], ships[bs_ptr->winner_id].bombs_num, ships[bs_ptr->winner_id].bombs_num > 1 ? "s" : "");

   return 0;
}
