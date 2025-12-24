#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
using namespace std;

vector < pair<int, int> > ans;
vector <int> arr, dp;
pair <int, int> vst;
int vst_cnt;

inline unsigned long long index(const int i, const int j) {return ((unsigned long long)j * (j - 1) >> 1) + i;}

int top_down(const int i, const int j) {
    unsigned long long pos = index(i, j);
    if (i >= j)
        return 0;
    if (dp[pos] != -1)
        return dp[pos];
    vst = make_pair(i, j);
    if (vst_cnt < 3) {
        printf("(%d,%d), ", i, j);
        vst_cnt++;
    }
    if (arr[j] != -1 && i <= arr[j] && arr[j] <= j)
        dp[pos] = max(dp[pos], top_down(i, arr[j] - 1) + 1 + top_down(arr[j] + 1, j - 1));
    dp[pos] = max(dp[pos], top_down(i, j - 1));
    return dp[pos];
}

void traceback(const int i, const int j) {
    unsigned long long pos = index(i, j);
    if (i >= j || dp[pos] <= 0)
        return;
    if (dp[pos] == dp[index(i, j - 1)])
        return traceback(i, j - 1);
    ans.push_back(make_pair(arr[j], j));
    traceback(i, arr[j] - 1);
    traceback(arr[j] + 1, j - 1);
    return;
}

int main(int argc, char* argv[]) {
    bool method = true;
    string input_file, output_file;
     for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "-td")
            method = false;
        else if (input_file.empty())
            input_file = arg;
        else
            output_file = arg;
    }
    ifstream fin(input_file);
    ofstream fout(output_file);
    unsigned long long n, sz;
    int a, b;
    fin >> n;
    arr.assign(n, -1);
    sz = (n * (n - 1)) >> 1;
    if (sz > dp.max_size())
        return cerr << "vector<T>::max_size() < needed\n", 1;
    for (int i = 0; i < n >> 1; i++) {
        fin >> a >> b;
        arr[max(a, b)] = min(a, b);
    }
    if (method) {
        dp.assign(sz, 0);
        for (int j = 1; j < n; j++)
            for (int i = j - 1; i >= 0; i--) {
                long long pos = index(i, j);
                dp[pos] = dp[index(i, j - 1)];
                if (arr[j] != -1 && i <= arr[j]) {
                    int val = 1;
                    if (i < arr[j] - 1)
                        val += dp[index(i, arr[j] - 1)];
                    if (arr[j] + 1 < j - 1)
                        val += dp[index(arr[j] + 1, j - 1)];
                    dp[pos] = max(dp[pos], val);
                }
                vst = make_pair(i, j);
                if (vst_cnt < 3) {
                    printf("(%d,%d), ", i, j);
                    vst_cnt++;
                }
            }
    }
    else {
        dp.assign(sz, -1);
        top_down(0, n - 1);
    }
    traceback(0, n - 1);
    sort(ans.begin(), ans.end());
    fout << ans.size() << '\n';
    for (const auto &i:ans)
        fout << i.first << ' ' << i.second << '\n';
    fin.close();
    fout.close();
    printf("..., (%d,%d)\n", vst.first, vst.second);
    return 0;
}
