/*
 ============================================================
   CPU SCHEDULING SIMULATOR  -  Mini OS Lab
   Algorithms : FCFS | SJF-NP | SJF-P (SRTF) | RR | Priority
   Features   : Gantt Chart | Wait Time | Turnaround | Compare
 ============================================================
*/

#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <string>
#include <climits>
#include <map>
#include <numeric>

using namespace std;

// ─────────────────────────────────────────────
//  ANSI Color Codes
// ─────────────────────────────────────────────
#define RESET    "\033[0m"
#define BOLD     "\033[1m"
#define RED      "\033[31m"
#define GREEN    "\033[32m"
#define YELLOW   "\033[33m"
#define MAGENTA  "\033[35m"
#define CYAN     "\033[36m"

const string BG_COLORS[] = {
    "\033[41m","\033[42m","\033[43m","\033[44m","\033[45m",
    "\033[46m","\033[101m","\033[102m","\033[103m","\033[104m"
};

// Repeat a multi-byte UTF-8 string n times
string rep(const string& s, int n) {
    string r; r.reserve(s.size() * n);
    for (int i = 0; i < n; i++) r += s;
    return r;
}


// ─────────────────────────────────────────────
//  Data Structures
// ─────────────────────────────────────────────
struct Process {
    int  pid, arrivalTime, burstTime, priority;
    int  remainingTime, startTime, finishTime;
    int  waitingTime, turnaroundTime;
    bool started;
    Process(int id, int at, int bt, int pr=0)
        : pid(id), arrivalTime(at), burstTime(bt), priority(pr),
          remainingTime(bt), startTime(-1), finishTime(0),
          waitingTime(0), turnaroundTime(0), started(false) {}
};

struct GanttEntry { int pid, start, end; };

struct Result {
    string algorithmName;
    double avgWT, avgTAT, cpuUtil;
    int    totalTime;
};

// ─────────────────────────────────────────────
//  UI Helpers
// ─────────────────────────────────────────────
void printHeader(const string& title) {
    int W = 62;
    cout << "\n" << BOLD << CYAN;
    cout << "+" << rep("=", W) << "+\n";
    string padded = "  " + title;
    cout << "|" << left << setw(W) << padded << "|\n";
    cout << "+" << rep("=", W) << "+\n";
    cout << RESET;
}

void printSeparator() {
    cout << CYAN << rep("-", 64) << "\n" << RESET;
}

// ─────────────────────────────────────────────
//  Gantt Chart
// ─────────────────────────────────────────────
void drawGanttChart(const vector<GanttEntry>& gantt, const string& name) {
    if (gantt.empty()) return;

    // Merge consecutive same-pid blocks
    vector<GanttEntry> m;
    for (const auto& g : gantt) {
        if (!m.empty() && m.back().pid == g.pid) m.back().end = g.end;
        else m.push_back(g);
    }

    // Assign colors
    map<int,int> pidClr;
    int ci = 0;
    for (const auto& g : m)
        if (!pidClr.count(g.pid)) pidClr[g.pid] = ci++ % 10;

    auto W = [](int dur) { return max(5, dur * 2); };

    cout << "\n" << BOLD << YELLOW << "  Gantt Chart [" << name << "]\n" << RESET;

    // Top border
    cout << "  ";
    for (const auto& g : m) cout << "+" << rep("-", W(g.end-g.start));
    cout << "+\n";

    // Labels
    cout << "  ";
    for (const auto& g : m) {
        int w   = W(g.end - g.start);
        string lbl = (g.pid == -1) ? "IDLE" : "P" + to_string(g.pid);
        int pad = w - (int)lbl.size();
        int lp  = pad/2, rp = pad - lp;
        cout << "|" << BG_COLORS[pidClr[g.pid]] << BOLD
             << string(lp,' ') << lbl << string(rp,' ') << RESET;
    }
    cout << "|\n";

    // Bottom border
    cout << "  ";
    for (const auto& g : m) cout << "+" << rep("-", W(g.end-g.start));
    cout << "+\n";

    // Time markers
    cout << "  ";
    for (size_t i = 0; i < m.size(); i++) {
        int w  = W(m[i].end - m[i].start);
        string t = to_string(m[i].start);
        cout << BOLD << t << RESET << string(w + 1 - (int)t.size(), ' ');
    }
    cout << BOLD << m.back().end << RESET << "\n";
}

// ─────────────────────────────────────────────
//  Results Table
// ─────────────────────────────────────────────
void printTable(const vector<Process>& procs, bool showPri) {
    cout << "\n" << BOLD << GREEN;
    string h4 = showPri ? " Priority " : "  Start   ";
    cout << "  +----+----------+-----------+" << h4 << "+------------+--------------+\n";
    cout << "  | P# | Arrival  |  Burst    |" << h4 << "|  Waiting   | Turnaround   |\n";
    cout << "  +----+----------+-----------+" << h4 << "+------------+--------------+\n";
    cout << RESET;
    for (const auto& p : procs) {
        int c4 = showPri ? p.priority : p.startTime;
        cout << "  | " << BOLD << "P" << left << setw(2) << p.pid << RESET
             << " | " << right << setw(8)  << p.arrivalTime
             << " | " << setw(9)  << p.burstTime
             << " | " << setw(8)  << c4
             << " | " << YELLOW  << setw(10) << p.waitingTime  << RESET
             << " | " << CYAN    << setw(12) << p.turnaroundTime << RESET << " |\n";
    }
    cout << GREEN
         << "  +----+----------+-----------+----------+------------+--------------+\n"
         << RESET;
}

// ─────────────────────────────────────────────
//  Metrics
// ─────────────────────────────────────────────
Result printMetrics(vector<Process>& procs, const string& name,
                    const vector<GanttEntry>& gantt, bool showPri=false) {
    int n = (int)procs.size();
    double tw=0, tt=0;
    for (const auto& p : procs) { tw += p.waitingTime; tt += p.turnaroundTime; }
    int   total    = gantt.empty() ? 0 : gantt.back().end;
    int   busy     = 0;
    for (const auto& g : gantt) if (g.pid != -1) busy += g.end - g.start;
    double util    = total > 0 ? 100.0 * busy / total : 0.0;
    double awt     = tw / n;
    double atat    = tt / n;

    printTable(procs, showPri);

    cout << "\n  " << BOLD << MAGENTA << "Performance Metrics\n" << RESET;
    cout << "  +------------------------------------------+\n";
    cout << "  |  " << BOLD << "Avg Waiting Time    : " << RESET
         << YELLOW << fixed << setprecision(2) << awt  << " units" << RESET << "\n";
    cout << "  |  " << BOLD << "Avg Turnaround Time : " << RESET
         << CYAN   << fixed << setprecision(2) << atat << " units" << RESET << "\n";
    cout << "  |  " << BOLD << "CPU Utilization     : " << RESET
         << GREEN  << fixed << setprecision(1) << util << "%" << RESET << "\n";
    cout << "  |  " << BOLD << "Total Makespan      : " << RESET
         << total << " units\n";
    cout << "  +------------------------------------------+\n";

    return {name, awt, atat, util, total};
}


// ═══════════════════════════════════════════════════════════
//  ALGORITHM 1 — FCFS
// ═══════════════════════════════════════════════════════════
Result runFCFS(vector<Process> procs) {
    printHeader("1. FIRST COME FIRST SERVE (FCFS)");
    sort(procs.begin(), procs.end(),
         [](const Process& a, const Process& b){ return a.arrivalTime < b.arrivalTime; });
    vector<GanttEntry> gantt;
    int t = 0;
    for (auto& p : procs) {
        if (t < p.arrivalTime) { gantt.push_back({-1, t, p.arrivalTime}); t = p.arrivalTime; }
        p.startTime = t;
        p.waitingTime = t - p.arrivalTime;
        t += p.burstTime;
        p.finishTime = t;
        p.turnaroundTime = t - p.arrivalTime;
        gantt.push_back({p.pid, p.startTime, p.finishTime});
    }
    drawGanttChart(gantt, "FCFS");
    return printMetrics(procs, "FCFS", gantt);
}

// ═══════════════════════════════════════════════════════════
//  ALGORITHM 2 — SJF Non-Preemptive
// ═══════════════════════════════════════════════════════════
Result runSJF_NP(vector<Process> procs) {
    printHeader("2. SJF  Non-Preemptive (SJF-NP)");
    int n = (int)procs.size();
    vector<bool> done(n, false);
    vector<GanttEntry> gantt;
    int t = 0, comp = 0;
    while (comp < n) {
        int idx = -1, minB = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!done[i] && procs[i].arrivalTime <= t) {
                if (procs[i].burstTime < minB) { minB = procs[i].burstTime; idx = i; }
                else if (procs[i].burstTime == minB && idx != -1
                         && procs[i].arrivalTime < procs[idx].arrivalTime) idx = i;
            }
        }
        if (idx == -1) {
            int na = INT_MAX;
            for (int i = 0; i < n; i++) if (!done[i]) na = min(na, procs[i].arrivalTime);
            gantt.push_back({-1, t, na}); t = na; continue;
        }
        procs[idx].startTime = t;
        procs[idx].waitingTime = t - procs[idx].arrivalTime;
        t += procs[idx].burstTime;
        procs[idx].finishTime = t;
        procs[idx].turnaroundTime = t - procs[idx].arrivalTime;
        gantt.push_back({procs[idx].pid, procs[idx].startTime, procs[idx].finishTime});
        done[idx] = true; comp++;
    }
    drawGanttChart(gantt, "SJF Non-Preemptive");
    return printMetrics(procs, "SJF-NP", gantt);
}

// ═══════════════════════════════════════════════════════════
//  ALGORITHM 3 — SJF Preemptive / SRTF
// ═══════════════════════════════════════════════════════════
Result runSJF_P(vector<Process> procs) {
    printHeader("3. SHORTEST REMAINING TIME FIRST  Preemptive (SRTF)");
    int n = (int)procs.size();
    vector<GanttEntry> gantt;
    int t = 0, comp = 0;
    while (comp < n) {
        int idx = -1, minR = INT_MAX;
        for (int i = 0; i < n; i++)
            if (procs[i].arrivalTime <= t && procs[i].remainingTime > 0)
                if (procs[i].remainingTime < minR) { minR = procs[i].remainingTime; idx = i; }
        if (idx == -1) {
            if (!gantt.empty() && gantt.back().pid == -1) gantt.back().end++;
            else gantt.push_back({-1, t, t+1});
            t++; continue;
        }
        if (!procs[idx].started) { procs[idx].startTime = t; procs[idx].started = true; }
        if (!gantt.empty() && gantt.back().pid == procs[idx].pid) gantt.back().end++;
        else gantt.push_back({procs[idx].pid, t, t+1});
        procs[idx].remainingTime--;
        t++;
        if (procs[idx].remainingTime == 0) {
            procs[idx].finishTime = t;
            procs[idx].turnaroundTime = t - procs[idx].arrivalTime;
            procs[idx].waitingTime = procs[idx].turnaroundTime - procs[idx].burstTime;
            comp++;
        }
    }
    drawGanttChart(gantt, "SJF Preemptive (SRTF)");
    return printMetrics(procs, "SJF-P / SRTF", gantt);
}

// ═══════════════════════════════════════════════════════════
//  ALGORITHM 4 — Round Robin
// ═══════════════════════════════════════════════════════════
Result runRoundRobin(vector<Process> procs, int quantum) {
    printHeader("4. ROUND ROBIN (RR)  —  Quantum = " + to_string(quantum));
    int n = (int)procs.size();
    sort(procs.begin(), procs.end(),
         [](const Process& a, const Process& b){ return a.arrivalTime < b.arrivalTime; });
    vector<GanttEntry> gantt;
    queue<int> rq;
    vector<bool> inQ(n, false);
    int t = 0, comp = 0, nxt = 0;
    while (nxt < n && procs[nxt].arrivalTime <= t) { rq.push(nxt); inQ[nxt++] = true; }
    while (comp < n) {
        if (rq.empty()) {
            int jump = procs[nxt].arrivalTime;
            gantt.push_back({-1, t, jump}); t = jump;
            while (nxt < n && procs[nxt].arrivalTime <= t) { rq.push(nxt); inQ[nxt++] = true; }
            continue;
        }
        int idx = rq.front(); rq.pop();
        if (!procs[idx].started) { procs[idx].startTime = t; procs[idx].started = true; }
        int ex = min(quantum, procs[idx].remainingTime);
        gantt.push_back({procs[idx].pid, t, t+ex});
        t += ex; procs[idx].remainingTime -= ex;
        while (nxt < n && procs[nxt].arrivalTime <= t) { rq.push(nxt); inQ[nxt++] = true; }
        if (procs[idx].remainingTime == 0) {
            procs[idx].finishTime = t;
            procs[idx].turnaroundTime = t - procs[idx].arrivalTime;
            procs[idx].waitingTime = procs[idx].turnaroundTime - procs[idx].burstTime;
            comp++;
        } else rq.push(idx);
    }
    string lbl = "Round Robin (Q=" + to_string(quantum) + ")";
    drawGanttChart(gantt, lbl);
    return printMetrics(procs, lbl, gantt);
}

// ═══════════════════════════════════════════════════════════
//  ALGORITHM 5 — Priority Scheduling
// ═══════════════════════════════════════════════════════════
Result runPriority(vector<Process> procs) {
    printHeader("5. PRIORITY SCHEDULING  (Non-Preemptive)");
    cout << YELLOW << "  Note: Lower number = Higher priority\n" << RESET;
    int n = (int)procs.size();
    vector<bool> done(n, false);
    vector<GanttEntry> gantt;
    int t = 0, comp = 0;
    while (comp < n) {
        int idx = -1, bestP = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!done[i] && procs[i].arrivalTime <= t) {
                if (procs[i].priority < bestP) { bestP = procs[i].priority; idx = i; }
                else if (procs[i].priority == bestP && idx != -1
                         && procs[i].arrivalTime < procs[idx].arrivalTime) idx = i;
            }
        }
        if (idx == -1) {
            int na = INT_MAX;
            for (int i = 0; i < n; i++) if (!done[i]) na = min(na, procs[i].arrivalTime);
            gantt.push_back({-1, t, na}); t = na; continue;
        }
        procs[idx].startTime = t;
        procs[idx].waitingTime = t - procs[idx].arrivalTime;
        t += procs[idx].burstTime;
        procs[idx].finishTime = t;
        procs[idx].turnaroundTime = t - procs[idx].arrivalTime;
        gantt.push_back({procs[idx].pid, procs[idx].startTime, procs[idx].finishTime});
        done[idx] = true; comp++;
    }
    drawGanttChart(gantt, "Priority Scheduling");
    return printMetrics(procs, "Priority", gantt, true);
}


// ═══════════════════════════════════════════════════════════
//  COMPARISON TABLE
// ═══════════════════════════════════════════════════════════
void printComparison(const vector<Result>& results) {
    cout << "\n\n";
    printHeader("ALGORITHM COMPARISON SUMMARY");

    double bestWT  = results[0].avgWT;
    double bestTAT = results[0].avgTAT;
    for (const auto& r : results) { bestWT = min(bestWT, r.avgWT); bestTAT = min(bestTAT, r.avgTAT); }

    cout << "\n" << BOLD;
    cout << "  +--------------------------+------------------+------------------+-------------+\n";
    cout << "  | Algorithm                | Avg Waiting Time | Avg Turnaround   | CPU Util %  |\n";
    cout << "  +--------------------------+------------------+------------------+-------------+\n";
    cout << RESET;

    for (const auto& r : results) {
        bool wb = (r.avgWT  == bestWT);
        bool tb = (r.avgTAT == bestTAT);
        cout << "  | " << left << setw(24) << r.algorithmName << " | ";
        if (wb)  cout << GREEN << BOLD;
        cout << right << fixed << setprecision(2) << setw(14) << r.avgWT  << "  ";
        if (wb)  cout << RESET;
        cout << "| ";
        if (tb)  cout << CYAN  << BOLD;
        cout << fixed << setprecision(2) << setw(14) << r.avgTAT << "  ";
        if (tb)  cout << RESET;
        cout << "| " << MAGENTA << fixed << setprecision(1) << setw(9) << r.cpuUtil << "%" << RESET << "  |\n";
    }
    cout << "  +--------------------------+------------------+------------------+-------------+\n";

    auto bw = min_element(results.begin(), results.end(),
        [](const Result& a, const Result& b){ return a.avgWT  < b.avgWT; });
    auto bt = min_element(results.begin(), results.end(),
        [](const Result& a, const Result& b){ return a.avgTAT < b.avgTAT; });

    cout << "\n  " << GREEN << BOLD << "  Best Avg Waiting Time    : " << RESET
         << GREEN << bw->algorithmName << " (" << fixed << setprecision(2) << bw->avgWT  << " units)\n" << RESET;
    cout << "  " << CYAN  << BOLD << "  Best Avg Turnaround Time : " << RESET
         << CYAN  << bt->algorithmName << " (" << fixed << setprecision(2) << bt->avgTAT << " units)\n" << RESET;

    // ── Bar Chart — Avg Waiting Time
    cout << "\n  " << BOLD << YELLOW << "Avg Waiting Time — Bar Chart\n" << RESET;
    double maxWT = 0; for (const auto& r : results) maxWT = max(maxWT, r.avgWT);
    for (const auto& r : results) {
        int bars = (maxWT > 0) ? (int)(r.avgWT / maxWT * 35) : 0;
        cout << "  " << left << setw(28) << r.algorithmName << " |"
             << (r.avgWT == bestWT ? GREEN : RED)
             << rep("#", bars) << RESET
             << " " << fixed << setprecision(2) << r.avgWT << "\n";
    }

    // ── Bar Chart — Avg Turnaround Time
    cout << "\n  " << BOLD << YELLOW << "Avg Turnaround Time — Bar Chart\n" << RESET;
    double maxTAT = 0; for (const auto& r : results) maxTAT = max(maxTAT, r.avgTAT);
    for (const auto& r : results) {
        int bars = (maxTAT > 0) ? (int)(r.avgTAT / maxTAT * 35) : 0;
        cout << "  " << left << setw(28) << r.algorithmName << " |"
             << (r.avgTAT == bestTAT ? CYAN : YELLOW)
             << rep("#", bars) << RESET
             << " " << fixed << setprecision(2) << r.avgTAT << "\n";
    }
}

// ═══════════════════════════════════════════════════════════
//  INPUT
// ═══════════════════════════════════════════════════════════
vector<Process> getInput(bool needPri) {
    int n; cout << BOLD << "\n  How many processes? " << RESET; cin >> n;
    vector<Process> v;
    for (int i = 0; i < n; i++) {
        int at, bt, pr = 0;
        cout << BOLD << "  P" << (i+1) << " -> Arrival: " << RESET; cin >> at;
        cout << BOLD << "  P" << (i+1) << " -> Burst  : " << RESET; cin >> bt;
        if (needPri) { cout << BOLD << "  P" << (i+1) << " -> Priority: " << RESET; cin >> pr; }
        v.emplace_back(i+1, at, bt, pr);
    }
    return v;
}

vector<Process> demo() {
    return {{1,0,6,3},{2,1,8,1},{3,2,7,4},{4,3,3,2},{5,4,4,5}};
}

void showDemo(const vector<Process>& p) {
    cout << "\n  " << BOLD << "Demo Processes (5 processes):\n" << RESET;
    cout << "  +----+---------+-------+----------+\n";
    cout << "  | P# | Arrival | Burst | Priority |\n";
    cout << "  +----+---------+-------+----------+\n";
    for (const auto& x : p)
        cout << "  | P" << x.pid << "  |    " << x.arrivalTime
             << "    |   " << x.burstTime << "   |    " << x.priority << "     |\n";
    cout << "  +----+---------+-------+----------+\n\n";
}

// ═══════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════
int main() {
    cout << BOLD << CYAN;
    cout << "\n  +================================================================+\n";
    cout << "  |                                                                |\n";
    cout << "  |        CPU SCHEDULING SIMULATOR  -  Mini OS Lab               |\n";
    cout << "  |                                                                |\n";
    cout << "  |    Algorithms : FCFS | SJF-NP | SRTF | Round Robin | Pri.     |\n";
    cout << "  |    Output     : Gantt Chart | Metrics | Comparison             |\n";
    cout << "  |                                                                |\n";
    cout << "  +================================================================+\n";
    cout << RESET;

    vector<Process>  procs;
    vector<Result>   allRes;
    bool loaded  = false;
    int  quantum = 2;

    while (true) {
        cout << BOLD << GREEN;
        cout << "\n  =========  MAIN MENU  =========\n" << RESET;
        cout << "   [1]  FCFS\n";
        cout << "   [2]  SJF Non-Preemptive\n";
        cout << "   [3]  SJF Preemptive / SRTF\n";
        cout << "   [4]  Round Robin\n";
        cout << "   [5]  Priority Scheduling\n";
        cout << "   [6]  Run ALL algorithms and Compare\n";
        cout << "   [7]  Load Demo Processes\n";
        cout << "   [8]  Enter New Processes\n";
        cout << "   [9]  Clear results\n";
        cout << "   [0]  Exit\n";
        cout << BOLD << "\n  Choice: " << RESET;

        int ch;
        if (!(cin >> ch)) { cin.clear(); cin.ignore(1000,'\n'); continue; }

        if (ch == 0) { cout << BOLD << CYAN << "\n  Goodbye!\n\n" << RESET; break; }

        if (ch == 7) {
            procs = demo(); loaded = true; allRes.clear(); showDemo(procs); continue;
        }
        if (ch == 8) {
            cout << "  Need priority values? (1=Yes / 0=No): ";
            int np; cin >> np;
            procs = getInput(np == 1); loaded = true; allRes.clear(); continue;
        }
        if (ch == 9) { allRes.clear(); cout << GREEN << "  Results cleared.\n" << RESET; continue; }

        // Ensure processes are loaded
        if (!loaded && ch >= 1 && ch <= 6) {
            cout << "  No processes loaded. Use demo? (1=Yes / 0=Enter manually): ";
            int d; cin >> d;
            if (d == 1) { procs = demo(); loaded = true; showDemo(procs); }
            else         { procs = getInput(ch == 5 || ch == 6); loaded = true; }
        }

        if (ch == 6) {
            cout << BOLD << "  Time Quantum for Round Robin: " << RESET; cin >> quantum;
            allRes.clear();
            printSeparator(); allRes.push_back(runFCFS(procs));
            printSeparator(); allRes.push_back(runSJF_NP(procs));
            printSeparator(); allRes.push_back(runSJF_P(procs));
            printSeparator(); allRes.push_back(runRoundRobin(procs, quantum));
            printSeparator(); allRes.push_back(runPriority(procs));
            printComparison(allRes);
        }
        else if (ch >= 1 && ch <= 5) {
            Result r = {"", 0, 0, 0, 0};
            if      (ch == 1) r = runFCFS(procs);
            else if (ch == 2) r = runSJF_NP(procs);
            else if (ch == 3) r = runSJF_P(procs);
            else if (ch == 4) {
                cout << BOLD << "  Time Quantum: " << RESET; cin >> quantum;
                r = runRoundRobin(procs, quantum);
            }
            else if (ch == 5) r = runPriority(procs);
            allRes.push_back(r);

            if (allRes.size() > 1) {
                cout << BOLD << "\n  Show comparison with previous runs? (1=Yes / 0=No): " << RESET;
                int s; cin >> s;
                if (s == 1) printComparison(allRes);
            }
        }
        else { cout << RED << "  Invalid choice.\n" << RESET; }
    }
    return 0;
}
