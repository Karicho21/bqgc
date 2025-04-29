#include <iostream>
#include <string>
#include <queue>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>
#include <stdexcept>
#include "rapidjson/error/error.h"
#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"
#include <rapidjson/document.h>
#include <chrono>

using namespace std;
using namespace rapidjson;

const bool DEBUG = true;

struct ParseException : std::runtime_error {
    rapidjson::ParseErrorCode code;
    size_t offset;

    ParseException(rapidjson::ParseErrorCode code, const char* msg, size_t offset) : 
        std::runtime_error(msg), 
        code(code),
        offset(offset) {}
};

const string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

template <typename T>
class BlockingQueue {
private:
    queue<T> queue_;
    mutex mutex_;
    condition_variable cond_;
    bool is_closed_ = false;
    atomic<int> active_workers_ = 0;
    atomic<int> unfinished_tasks_ = 0;

public:
    void push(const T& item) {
        unique_lock<mutex> lock(mutex_);
        queue_.push(item);
        unfinished_tasks_++;
        //if (DEBUG) cout << "Pushed item, unfinished tasks: " << unfinished_tasks_ << endl;
        cond_.notify_one();
    }

    bool pop(T& item) {
        unique_lock<mutex> lock(mutex_);
        active_workers_++;
        
        while (queue_.empty() && !is_closed_) {
            //if (DEBUG) cout << "Worker waiting..." << endl;
            cond_.wait(lock);
        }
        
        if (queue_.empty()) {
            active_workers_--;
            //if (DEBUG) cout << "Worker exiting (queue empty and closed)" << endl;
            return false;
        }
        
        item = queue_.front();
        queue_.pop();
        return true;
    }

    void task_done() {
        unfinished_tasks_--;
        active_workers_--;
        //if (DEBUG) cout << "Task done, unfinished: " << unfinished_tasks_ << ", active workers: " << active_workers_ << endl;
        if (unfinished_tasks_ == 0) {
            cond_.notify_all();
        }
    }

    void close() {
        unique_lock<mutex> lock(mutex_);
        is_closed_ = true;
        cond_.notify_all();
    }

    void join() {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this]() { 
            return unfinished_tasks_ == 0; 
        });
    }
};

string url_encode(CURL* curl, string input) {
    char* out = curl_easy_escape(curl, input.c_str(), input.size());
    string s = out;
    curl_free(out);
    return s;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

string fetch_neighbors(CURL* curl, const string& node) {
    string url = SERVICE_URL + url_encode(curl, node);
    string response;

    //if (DEBUG) cout << "Fetching: " << url << endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: C++-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        cerr << "CURL error (" << node << "): " << curl_easy_strerror(res) << endl;
    }

    curl_slist_free_all(headers);

    return (res == CURLE_OK) ? response : "{}";
}

vector<string> get_neighbors(const string& json_str) {
    vector<string> neighbors;
    Document doc;
    doc.Parse(json_str.c_str());
    
    if (doc.HasParseError()) {
        throw ParseException(
            doc.GetParseError(), 
            rapidjson::GetParseError_En(doc.GetParseError()), 
            doc.GetErrorOffset()
        );
    }
    
    if (doc.HasMember("neighbors") && doc["neighbors"].IsArray()) {
        for (const auto& neighbor : doc["neighbors"].GetArray()) {
            if (neighbor.IsString()) {
                neighbors.push_back(neighbor.GetString());
            }
        }
    }
    
    return neighbors;
}

void worker(
    CURL* curl,
    BlockingQueue<pair<string, int>>& queue,
    unordered_set<string>& visited,
    mutex& visited_mutex,
    vector<string>& result,
    mutex& result_mutex,
    int max_depth
) {
    pair<string, int> item;
    while (queue.pop(item)) {
        string node = item.first;
        int level = item.second;
        
        if (level <= max_depth) {
            lock_guard<mutex> result_lock(result_mutex);
            result.push_back(node);
        }
        
        if (level < max_depth) {
            try {
                vector<string> neighbors = get_neighbors(fetch_neighbors(curl, node));
                
                for (const auto& neighbor : neighbors) {
                    lock_guard<mutex> visited_lock(visited_mutex);
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        queue.push({neighbor, level + 1});
                    }
                }
            } catch (const exception& e) {
                cerr << "Error processing node " << node << ": " << e.what() << endl;
            }
        }
        
        queue.task_done();
    }
}

vector<string> parallel_bfs(const string& start, int depth, int num_threads = 4) {
    BlockingQueue<pair<string, int>> queue;
    unordered_set<string> visited;
    mutex visited_mutex;
    vector<string> result;
    mutex result_mutex;
    vector<thread> threads;
    
    vector<CURL*> curls(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        curls[i] = curl_easy_init();
        if (!curls[i]) {
            cerr << "Failed to initialize CURL for thread " << i << endl;
            for (int j = 0; j < i; ++j) {
                curl_easy_cleanup(curls[j]);
            }
            return {};
        }
    }
    
    {
        lock_guard<mutex> visited_lock(visited_mutex);
        visited.insert(start);
    }
    queue.push({start, 0});
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, 
                           curls[i], 
                           ref(queue), 
                           ref(visited), 
                           ref(visited_mutex),
                           ref(result), 
                           ref(result_mutex),
                           depth);
    }

    queue.join();

    queue.close();
    
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    for (auto curl : curls) {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
    
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) { 
        cerr << "Please enter : " << argv[0] << " <node_name> <depth> [thread_count]\n";
        cerr << "Example: " << argv[0] << " \"Tom Hanks\" 2 4\n";
        return 1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    string start_node = argv[1];
    int depth;
    try {
        depth = stoi(argv[2]);
    } catch (const exception& e) {
        cerr << "Error: Depth must be an integer.\n";
        curl_global_cleanup();
        return 1;
    }

    int num_threads = 8;
    
    if (argc == 4) {
        try {
            num_threads = stoi(argv[3]);
        } catch (const exception& e) {
            cerr << "Error: Thread count must be an integer. Using default 8 threads.\n";
        }
    }

    cout << "\n------------------- Starting BFS from: " << start_node << " with depth " << depth << " ------------------- \n" << endl;

    const auto start{chrono::steady_clock::now()};
    vector<string> nodes = parallel_bfs(start_node, depth, num_threads);
    const auto finish{chrono::steady_clock::now()};
    const chrono::duration<double> elapsed_seconds{finish - start};
    
    cout << "Results:\n";
    for (const auto& node : nodes)
        cout << "- " << node << "  ";

    cout << "\n\n------------------------------------------------------\n";
    cout << "Time passed: " << elapsed_seconds.count() << " seconds\n";
    cout << "Nodes found: " << nodes.size() << endl;
    cout << "------------------------------------------------------\n\n";
    curl_global_cleanup();
    
    return 0;
}
