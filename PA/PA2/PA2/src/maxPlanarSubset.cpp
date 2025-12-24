#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
using namespace std;

int vst_count = 0;
pair<int,int> last_pair;
inline unsigned long long index(int i, int j) {
    return ((unsigned long long)j * (j + 1) >> 1) + i;
}
inline int dp_get(const vector<int>& dp, int i, int j) {
    if (i >= j) return 0;
    return dp[index(i, j)];
}

static void reconstruct(const vector<int>& chords,
                        const vector<int>& dp,
                        int i, int j,
                        vector<pair<int,int>>& ans) {
    if (i >= j) return;
    int k = chords[j];

    // if k not in [i, j], j can't be taken
    if (k < i || k > j) {
        reconstruct(chords, dp, i, j - 1, ans);
        return;
    }

    int skip = dp_get(dp, i, j - 1);
    int left  = dp_get(dp, i, k - 1);
    int right = dp_get(dp, k + 1, j - 1);
    int take = left + 1 + right;

    if (take > skip) {
        reconstruct(chords, dp, i, k - 1, ans);
        reconstruct(chords, dp, k + 1, j - 1, ans);
        ans.emplace_back(min(k, j), max(k, j));
    } else {
        reconstruct(chords, dp, i, j - 1, ans);
    }
}

int td_compute(const vector<int>& chords, int i, int j, vector<int>& dp)
{
    if(vst_count < 3){
        printf("(%d,%d), ", i, j);
        vst_count++;
    }
    if (i >= j) return 0;
    unsigned long long pos = index(i, j);
    if (dp[pos] != -1) return dp[pos];
    last_pair = make_pair(i, j);
    int k = chords[j];
    if (k == i) {
        dp[pos] = td_compute(chords, i + 1, j - 1, dp) + 1;
    } else if (k < i || k > j) {
        dp[pos] = td_compute(chords, i, j - 1, dp);
    } else {
        int b = td_compute(chords, i, k - 1, dp) + td_compute(chords, k + 1, j - 1, dp) + 1;
        int a = td_compute(chords, i, j - 1, dp);
        dp[pos] = max(a, b);
    }
    return dp[pos];
}

vector<pair<int,int>> tdMPS(const vector<int>& chords){
    int n = chords.size();
    vector<pair<int,int>> result;
    unsigned long long sz = ((unsigned long long)n * (n + 1)) >> 1; // triangular size
    vector<int> dp(sz, -1);
    td_compute(chords, 0, n - 1, dp);
    reconstruct(chords, dp, 0, n - 1, result);
    sort(result.begin(), result.end());
    return result;
}

vector<pair<int,int>> buMPS(const vector<int>& chords){
    int n = chords.size();
    unsigned long long sz = ((unsigned long long)n * (n + 1)) >> 1; // triangular size
    vector<int> dp(sz, 0);
    // compute dp table for lengths l = 1..n-1 (only i<j)
    // j 從 1..n-1（因為區間長度至少為 1），對每個 j，i 走 0..j-1
for (int j = 1; j < n; ++j) {
    for (int i = 0; i < j; ++i) {
        if (vst_count < 3) {
            printf("(%d,%d), ", i, j);
            ++vst_count;
        }

        int k = chords[j];
        unsigned long long pos = index(i, j);
        if (k < i || k >= j) {
            dp[pos] = dp_get(dp, i, j - 1);
        }
        else if (k == i) {
            dp[pos] = dp_get(dp, i + 1, j - 1) + 1;
        }
        
        else {
            int left  = dp_get(dp, i,     k - 1);
            int right = dp_get(dp, k + 1, j - 1);
            dp[pos] = max(dp_get(dp, i, j - 1), left + right + 1);
        }
    }
}

    // reconstruct solution
    vector<pair<int,int>> result;
    reconstruct(chords, dp, 0, n - 1, result);
    sort(result.begin(), result.end());
    return result;
}
int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 3) {
        cerr << "Usage: ./mps <input> <output> [--method=td|bu]\n";
        return 1;
    }
    string method = "bu";
    string inputFile, outputFile;

    if (argc == 4) {
        // 旗標放在最前面
        string arg = argv[1];
        if (arg.rfind("--method=", 0) != 0) { return 1; }
        string m = arg.substr(9);
        if (m == "td" || m == "bu") method = m;
        else { cerr << "Unknown method: " << m << "\n"; return 1; }
        inputFile = argv[2];
        outputFile = argv[3];
    } else if (argc == 3) {
        // 沒帶 method，預設 bu
        inputFile = argv[1];
        outputFile = argv[2];
    } else {
        cerr << "Usage: ./mps <input> <output> [--method=td|bu]\n";
        return 1;
    }

    ifstream fin(inputFile);
    ofstream fout(outputFile);
    if (!fin || !fout) { cerr << "Error opening file.\n"; return 1; }

    int n;
    fin >> n;
    vector<int> chords(n);
    for (int i = 0; i < n; ++i) {
        int pre = 0;
        int back = 0;

        fin >> pre >> back;
        chords[pre] = back;
        chords[back] = pre;
    }
    // int zero;
    // fin >> zero;

    vector<pair<int, int>> result;
    if (method == "td") {
        result = tdMPS(chords);
    } else {
        result = buMPS(chords);
    }
    printf("..., (%d,%d)\n", last_pair.first, last_pair.second);
    fout << result.size() << "\n";
    for (const auto& chord : result) {
        fout << chord.first << " " << chord.second << "\n";
    }
}