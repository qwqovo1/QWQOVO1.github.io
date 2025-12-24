#define CURRENT_FILE_MAIN
#ifdef CURRENT_FILE_MAIN
#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <climits>
#include <cmath>
#include <tuple>

struct Run {
    int start, len;
    Run(int s, int l) : start(s), len(l) {}
};

// 插入排序 [l, r)
void insertion_sort(std::vector<int>& arr, int l, int r) {
    for (int i = l + 1; i < r; ++i) {
        int key = arr[i];
        int j = i - 1;
        while (j >= l && arr[j] > key) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;
    }
}

// 反转 [l, r)
void reverse_range(std::vector<int>& arr, int l, int r) {
    std::reverse(arr.begin() + l, arr.begin() + r);
}

// 查找下一个 run 的结束位置
int find_run_end(const std::vector<int>& arr, int start, int n) {
    if (start >= n - 1) return n;
    int end = start + 2;
    if (arr[start] > arr[start + 1]) {
        // 严格递减
        while (end < n && arr[end - 1] > arr[end]) ++end;
        return end;
    }
    else {
        // 非递减（允许相等）
        while (end < n && arr[end - 1] <= arr[end]) ++end;
        return end;
    }
}

// 败者树节点
struct LoserTree {
    std::vector<int> losers;
    std::vector<int> keys;
    std::vector<Run> runs;
    std::vector<int> indices; // 当前每个 run 的指针
    int k;

    LoserTree(const std::vector<Run>& r, const std::vector<int>& arr)
        : runs(r), k(static_cast<int>(r.size())), losers(k, -1), keys(k), indices(k, 0) {
        for (int i = 0; i < k; ++i) {
            keys[i] = (indices[i] < runs[i].len) ? arr[runs[i].start + indices[i]] : INT_MAX;
        }
        build(arr);
    }

    void build(const std::vector<int>& arr) {
        for (int i = 0; i < k; ++i) {
            adjust(i, arr);
        }
    }

    void adjust(int id, const std::vector<int>& arr) {
        int parent = (id + k) / 2;
        while (parent > 0) {
            if (keys[id] > keys[losers[parent]]) {
                std::swap(losers[parent], id);
            }
            parent /= 2;
        }
        losers[0] = id;
    }

    int get_min_index() const { return losers[0]; }
    bool done() const { return keys[losers[0]] == INT_MAX; }

    void advance(int idx, const std::vector<int>& arr) {
        indices[idx]++;
        if (indices[idx] >= runs[idx].len) {
            keys[idx] = INT_MAX;
        }
        else {
            keys[idx] = arr[runs[idx].start + indices[idx]];
        }
        adjust(idx, arr);
    }
};

// 双路归并：从 src 归并到 dst
void merge_two(const std::vector<int>& src, std::vector<int>& dst,
    const Run& a, const Run& b) {
    int i = a.start, j = b.start;
    int end_i = i + a.len, end_j = j + b.len;
    int out = a.start;

    // 首尾检查优化
    if (src[end_i - 1] <= src[j]) {
        std::copy(src.begin() + a.start, src.begin() + end_i, dst.begin() + out);
        std::copy(src.begin() + j, src.begin() + end_j, dst.begin() + out + a.len);
        return;
    }
    if (src[end_j - 1] <= src[i]) {
        std::copy(src.begin() + j, src.begin() + end_j, dst.begin() + out);
        std::copy(src.begin() + i, src.begin() + end_i, dst.begin() + out + b.len);
        return;
    }

    while (i < end_i && j < end_j) {
        if (src[i] <= src[j]) {
            dst[out++] = src[i++];
        }
        else {
            dst[out++] = src[j++];
        }
    }
    while (i < end_i) dst[out++] = src[i++];
    while (j < end_j) dst[out++] = src[j++];
}

// 基于败者树的多路归并（写入 dst）
void loser_tree_merge(const std::vector<Run>& runs,
    const std::vector<int>& src,
    std::vector<int>& dst) {
    LoserTree lt(runs, src);
    int out = runs.front().start;

    while (!lt.done()) {
        int winner = lt.get_min_index();
        int val = src[runs[winner].start + lt.indices[winner]];
        dst[out++] = val;
        lt.advance(winner, src);
    }
}

// 堆归并（兼容 C++11）
void heap_merge(const std::vector<Run>& runs,
    const std::vector<int>& src,
    std::vector<int>& dst) {
    using Node = std::tuple<int, int, int>; // (value, run_id, index_in_run)
    auto cmp = [](const Node& a, const Node& b) { return std::get<0>(a) > std::get<0>(b); };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> pq(cmp);

    int out = runs.front().start;
    for (int i = 0; i < static_cast<int>(runs.size()); ++i) {
        if (runs[i].len > 0) {
            pq.push(std::make_tuple(src[runs[i].start], i, 0));
        }
    }

    while (!pq.empty()) {
        Node top = pq.top(); pq.pop();
        int val = std::get<0>(top);
        int rid = std::get<1>(top);
        int idx = std::get<2>(top);

        dst[out++] = val;

        if (idx + 1 < runs[rid].len) {
            pq.push(std::make_tuple(src[runs[rid].start + idx + 1], rid, idx + 1));
        }
    }
}

// 判断是否应合并栈顶 runs（Timsort 启发式简化版）
bool should_merge(const std::vector<Run>& stack) {
    int n = static_cast<int>(stack.size());
    if (n < 2) return false;
    if (n == 2) return stack[n - 2].len <= stack[n - 1].len;
    // A <= B + C 或 B <= C
    return (stack[n - 3].len <= stack[n - 2].len + stack[n - 1].len) ||
        (stack[n - 2].len <= stack[n - 1].len);
}

// 合并栈顶两个 runs（使用 tmp 作为临时缓冲）
void merge_top_runs(std::vector<Run>& stack,
    std::vector<int>& arr,
    std::vector<int>& tmp) {
    Run b = stack.back(); stack.pop_back();
    Run a = stack.back(); stack.pop_back();

    merge_two(arr, tmp, a, b);

    // 拷回 arr
    int start = a.start;
    int len = a.len + b.len;
    std::copy(tmp.begin() + start, tmp.begin() + start + len, arr.begin() + start);

    stack.emplace_back(start, len);
}

// 主排序函数
void adaptive_run_merge_sort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    if (n <= 1) return;

    // 1. 计算 minrun
    int minrun;
    if (n < 64) {
        minrun = n;
    }
    else {
        minrun = static_cast<int>(std::floor(std::log2(n)));
        minrun = std::max(3, minrun);
    }

    // 2. 生成 runs
    std::vector<Run> runs;
    int i = 0;
    while (i < n) {
        int end = find_run_end(arr, i, n);
        bool descending = (end - i > 1 && arr[i] > arr[i + 1]);
        if (descending) {
            reverse_range(arr, i, end);
        }
        int run_len = end - i;
        if (run_len < minrun) {
            int extend = std::min(i + minrun, n);
            insertion_sort(arr, i, extend);
            run_len = extend - i;
            i = extend;
        }
        else {
            i = end;
        }
        runs.emplace_back(i - run_len, run_len);
    }

    // 3. 用栈动态合并
    std::vector<Run> run_stack;
    std::vector<int> tmp = arr; // 辅助缓冲区

    for (auto& run : runs) {
        run_stack.push_back(run);
        while (should_merge(run_stack)) {
            merge_top_runs(run_stack, arr, tmp);
        }
    }

    // 4. 最终归并
    if (run_stack.size() == 1) return;

    if (run_stack.size() == 2) {
        merge_two(arr, tmp, run_stack[0], run_stack[1]);
        arr = std::move(tmp);
        return;
    }

    // 多路归并
    if (run_stack.size() <= 64) {
        loser_tree_merge(run_stack, arr, tmp);
    }
    else {
        heap_merge(run_stack, arr, tmp);
    }
    arr = std::move(tmp);
}

// 测试用例
int main() {
    std::vector<int> arr = { 5, 2, 9, 1, 5, 6, 3, 8, 7, 4 };
    std::cout << "Original: ";
    for (int x : arr) std::cout << x << " ";
    std::cout << "\n";

    adaptive_run_merge_sort(arr);

    std::cout << "Sorted:   ";
    for (int x : arr) std::cout << x << " ";
    std::cout << "\n";

    return 0;
}
#endif