#include "ThreadPool.hpp"
#include "barrier.hpp"
#include "dispatchTask.hpp"
#include "init.hpp"
#include "initTask.hpp"
#include "resources.hpp"
#include "sha256.c"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>
using namespace std::chrono;

void sleep(std::chrono::microseconds d)
{
    std::this_thread::sleep_for(d);
}

void hash(unsigned task_id, std::array<uint64_t, 8>& val)
{
    val[0] += task_id;

    uint32_t state[8]
        = {0x6a09'e667, 0xbb67'ae85, 0x3c6e'f372, 0xa54f'f53a, 0x510e'527f, 0x9b05'688c, 0x1f83'd9ab, 0x5be0'cd19};

    sha256_process(state, (uint8_t*) &val[0], sizeof(val));
}

std::chrono::microseconds task_duration(2);
unsigned n_resources = 16;
unsigned n_tasks = 128;
unsigned n_threads = 8;
unsigned min_dependencies = 0;
unsigned max_dependencies = 5;
std::mt19937 gen;

std::vector<std::vector<unsigned>> access_pattern;
std::vector<std::array<uint64_t, 8>> expected_hash;

void generate_access_pattern()
{
    std::uniform_int_distribution<unsigned> distrib_n_deps(min_dependencies, max_dependencies);
    std::uniform_int_distribution<unsigned> distrib_resource(0, n_resources - 1);

    access_pattern = std::vector<std::vector<unsigned>>(n_tasks);
    expected_hash = std::vector<std::array<uint64_t, 8>>(n_resources);
    std::vector<unsigned> path_length(n_resources);

    for(unsigned i = 0; i < n_tasks; ++i)
    {
        unsigned n_dependencies = distrib_n_deps(gen);
        for(unsigned j = 0; j < n_dependencies; ++j)
        {
            unsigned max_path_length = 0;

            while(1)
            {
                unsigned resource_id = distrib_resource(gen);
                if(std::find(access_pattern[i].begin(), access_pattern[i].end(), resource_id)
                   == access_pattern[i].end())
                {
                    access_pattern[i].push_back(resource_id);
                    hash(i, expected_hash[resource_id]);

                    if(path_length[resource_id] > max_path_length)
                        max_path_length = path_length[resource_id];

                    break;
                }
            }

            for(unsigned rid : access_pattern[i])
                path_length[rid] = max_path_length + 1;
        }
    }

    unsigned max_path_length = 1;
    for(unsigned pl : path_length)
        if(pl > max_path_length)
            max_path_length = pl;

    std::cout << "max path length = " << max_path_length << std::endl;
}

template<typename Container>
void print(Container c)
{
    std::cout << "{ ";
    for(auto& i : c)
    {
        std::cout << i << " ";
    }
    std::cout << "}";
}

auto test(rg::ThreadPool* ptr) -> rg::InitTask<int>
{
    std::vector<rg::Resource<std::shared_ptr<std::array<uint64_t, 8>>>> resources(n_resources);

    // Default initialize each element in-place
    for(auto& res : resources)
    {
        res = rg::Resource(std::make_shared<std::array<uint64_t, 8>>());
    }

    for(unsigned i = 0; i < n_tasks; ++i)
        switch(access_pattern[i].size())
        {
        case 0:
            {
                auto asdaf1 = co_await rg::dispatch_task(
                    []() -> rg::Task<int>
                    {
                        sleep(task_duration);
                        co_return 0;
                    });
                break;
            }
        case 1:
            {
                co_await rg::dispatch_task(
                    [](auto ra1, auto i) -> rg::Task<int>
                    {
                        std::cout << "started work with i: " << i << std::endl;
                        sleep(task_duration);
                        hash(i, *ra1);
                        co_return 0;
                    },
                    resources[access_pattern[i][0]].rg_write(),
                    i);
                break;
            }
        case 2:
            {
                auto asdaf3 = co_await rg::dispatch_task(
                    [](auto ra1, auto ra2, auto i) -> rg::Task<int>
                    {
                        sleep(task_duration);
                        hash(i, *ra1);
                        hash(i, *ra2);
                        co_return 0;
                    },
                    resources[access_pattern[i][0]].rg_write(),
                    resources[access_pattern[i][1]].rg_write(),
                    i);
                break;
            }
        case 3:
            {
                auto asdaf4 = co_await rg::dispatch_task(
                    [](auto ra1, auto ra2, auto ra3, auto i) -> rg::Task<int>
                    {
                        sleep(task_duration);
                        hash(i, *ra1);
                        hash(i, *ra2);
                        hash(i, *ra3);
                        co_return 0;
                    },
                    resources[access_pattern[i][0]].rg_write(),
                    resources[access_pattern[i][1]].rg_write(),
                    resources[access_pattern[i][2]].rg_write(),
                    i);
                break;
            }
        case 4:
            {
                auto asdaf5 = co_await rg::dispatch_task(
                    [](auto ra1, auto ra2, auto ra3, auto ra4, auto i) -> rg::Task<int>
                    {
                        sleep(task_duration);
                        hash(i, *ra1);
                        hash(i, *ra2);
                        hash(i, *ra3);
                        hash(i, *ra4);
                        co_return 0;
                    },
                    resources[access_pattern[i][0]].rg_write(),
                    resources[access_pattern[i][1]].rg_write(),
                    resources[access_pattern[i][2]].rg_write(),
                    resources[access_pattern[i][3]].rg_write(),
                    i);
                break;
            }
        case 5:
            {
                auto asdaf6 = co_await rg::dispatch_task(
                    [](auto ra1, auto ra2, auto ra3, auto ra4, auto ra5, auto i) -> rg::Task<int>
                    {
                        sleep(task_duration);
                        hash(i, *ra1);
                        hash(i, *ra2);
                        hash(i, *ra3);
                        hash(i, *ra4);
                        hash(i, *ra5);
                        co_return 0;
                    },
                    resources[access_pattern[i][0]].rg_write(),
                    resources[access_pattern[i][1]].rg_write(),
                    resources[access_pattern[i][2]].rg_write(),
                    resources[access_pattern[i][3]].rg_write(),
                    resources[access_pattern[i][4]].rg_write(),
                    i);
                break;
            }
        }

    std::cout << "tasks created" << std::endl;
    co_await BarrierAwaiter{};
    std::cout << "starting check" << std::endl;

    for(unsigned i{0}; i < n_resources; ++i)
    {
        std::cout << "loop i = " << i << std::endl;
        auto f = [](auto res, auto i) -> rg::Task<int>
        {
            std::cout << "copied i = " << i << " and address " << &i << std::endl;
            std::cout << "res address " << res.get() << std::endl;

            if(*res != expected_hash[i])
            {
                std::cout << "incorrect result! For resource " << i << " got ";
                print(*res);
                std::cout << " expected ";
                print(expected_hash[i]);
                std::cout << std::endl;
            }
            else
            {
                std::cout << "Correct result for resource " << i << "!" << std::endl;
            }
            co_return 0;
        };
        std::cout << "lambda copy done" << std::endl;

        co_await rg::dispatch_task(f, resources[i].rg_read(), i);
    }

    // std::this_thread::sleep_for(std::chrono::seconds(15));
    co_return 0;
}

int main()
{
    // spdlog::set_pattern("[thread %t] %^[%l]%$ %v");

    generate_access_pattern();

    auto poolObj = rg::init(n_threads);
    auto a = test(poolObj.pool_ptr());
    a.finalize();
    std::cout << "finalize done" << std::endl;
    return 0;
}
