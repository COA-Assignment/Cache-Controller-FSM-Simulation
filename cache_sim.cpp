#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

// ── request types ───────────────────────────────────────────────────
enum class ReqType { Read, Write };

// ── FSM states ───────────────────────
enum class State
{
    Idle,
    CompareTag,
    ReadHit,
    WriteHit,
    Miss,
    CheckDirty,
    WriteBack,
    MemWrite,
    MemWaitWrite1,
    MemWaitWrite2,
    Allocate,
    MemRead,
    MemWaitRead1,
    MemWaitRead2,
    UpdateCacheBlock,
    UpdateTag,
    SetValid,
    CheckWriteAfterAlloc,
    WriteAfterAlloc,
    UpdateCache,
    SetDirty,
    Done
};

// ── data structures ─────────────────────────────────────────────────
struct Request  { ReqType type{ReqType::Read}; uint32_t addr{0}; uint32_t data{0}; };
struct CacheLine { bool valid{false}; bool dirty{false}; uint32_t tag{0}; uint32_t data{0}; };

struct Signals
{
    bool cpu_req_ready{false};
    bool cpu_stall{false};
    bool cache_hit{false};
    bool cache_miss{false};
    bool mem_rd{false};
    bool mem_wr{false};
    bool mem_ready{false};
    bool mem_data_valid{false};
    bool data_out_valid{false};
    uint32_t data_out{0};
    string note;
};

struct TraceRow { int cycle{0}; State state{State::Idle}; Signals sig; string request_text; };

struct Stats
{
    uint64_t cycles{0};
    uint64_t requests{0};
    uint64_t reads{0};
    uint64_t writes{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t writebacks{0};
};

// ── helpers ─────────────────────────────────────────────────────────
string state_name(State s)
{
    switch (s)
    {
    case State::Idle:                 return "IDLE";
    case State::CompareTag:           return "COMPARE_TAG";
    case State::ReadHit:              return "READ_HIT";
    case State::WriteHit:             return "WRITE_HIT";
    case State::Miss:                 return "MISS";
    case State::CheckDirty:           return "CHECK_DIRTY";
    case State::WriteBack:            return "WRITE_BACK";
    case State::MemWrite:             return "MEM_WRITE";
    case State::MemWaitWrite1:        return "MEM_WAIT_WRITE_1";
    case State::MemWaitWrite2:        return "MEM_WAIT_WRITE_2";
    case State::Allocate:             return "ALLOCATE";
    case State::MemRead:              return "MEM_READ";
    case State::MemWaitRead1:         return "MEM_WAIT_READ_1";
    case State::MemWaitRead2:         return "MEM_WAIT_READ_2";
    case State::UpdateCacheBlock:     return "UPDATE_CACHE_BLOCK";
    case State::UpdateTag:            return "UPDATE_TAG";
    case State::SetValid:             return "SET_VALID";
    case State::CheckWriteAfterAlloc: return "CHECK_WRITE_AFTER_ALLOC";
    case State::WriteAfterAlloc:      return "WRITE_AFTER_ALLOC";
    case State::UpdateCache:          return "UPDATE_CACHE";
    case State::SetDirty:             return "SET_DIRTY";
    case State::Done:                 return "DONE";
    }
    return "UNKNOWN";
}

string req_name(ReqType t) { return t == ReqType::Read ? "READ" : "WRITE"; }

string hex32(uint32_t v)
{
    stringstream ss;
    ss << "0x" << uppercase << hex << v;
    return ss.str();
}

uint32_t parse_u32(const string &tok) { return static_cast<uint32_t>(stoul(tok, nullptr, 0)); }

// ── request loading ─────────────────────────────────────────────────
vector<Request> default_requests()
{
    return {
        {ReqType::Read,  0x00, 0},
        {ReqType::Read,  0x00, 0},
        {ReqType::Write, 0x00, 777},
        {ReqType::Read,  0x04, 0},
        {ReqType::Write, 0x08, 888},
        {ReqType::Write, 0x08, 999},
        {ReqType::Read,  0x0C, 0},
    };
}

vector<Request> load_requests(const string &path)
{
    if (path.empty()) return default_requests();

    ifstream in(path);
    if (!in.is_open())
    {
        cerr << "Warning: failed to open '" << path << "'. Using defaults.\n";
        return default_requests();
    }

    vector<Request> reqs;
    string line;
    while (getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        stringstream ss(line);
        string op, addr_text, data_text;
        ss >> op >> addr_text;
        if (op.empty() || addr_text.empty()) continue;

        Request r;
        r.type = (op == "W" || op == "w" || op == "WRITE" || op == "write")
                     ? ReqType::Write : ReqType::Read;
        r.addr = parse_u32(addr_text);
        if (r.type == ReqType::Write) { ss >> data_text; r.data = data_text.empty() ? 0U : parse_u32(data_text); }
        reqs.push_back(r);
    }

    if (reqs.empty())
    {
        cerr << "Warning: no valid requests in '" << path << "'. Using defaults.\n";
        return default_requests();
    }
    return reqs;
}

// ── main memory ─────────────────────────────────────────────────────
class MainMemory
{
public:
    explicit MainMemory(size_t words) : words_(words)
    {
        for (size_t i = 0; i < words_.size(); ++i)
            words_[i] = static_cast<uint32_t>(1000 + i * 10);
    }
    uint32_t read_word(uint32_t a) const { return words_.at(a % words_.size()); }
    void write_word(uint32_t a, uint32_t v)   { words_.at(a % words_.size()) = v; }
private:
    vector<uint32_t> words_;
};

// ── cache controller FSM ────────────────────────────────────────────
class CacheController
{
public:
    explicit CacheController(size_t num_lines) : lines_(num_lines) {}

    bool ready_for_request() const
    {
        return state_ == State::Idle && !pending_.has_value();
    }

    // One cycle of the FSM.  Records the state we are IN this cycle,
    // computes outputs, then transitions to the next state.
    TraceRow tick(int cycle, optional<Request> incoming, MainMemory &mem)
    {
        Signals sig;
        State cur = state_;          // state we are IN this cycle

        // ── signals (Moore-style: depend only on current state) ─────
        switch (cur)
        {
        case State::Idle:
            sig.cpu_req_ready = !pending_.has_value();
            break;
        case State::CompareTag:
            sig.cpu_stall = true;
            break;
        case State::ReadHit:
            sig.cache_hit  = true;
            sig.cpu_stall  = true;
            sig.data_out_valid = true;
            sig.data_out   = current_line_->data;
            sig.note       = "data=" + to_string(current_line_->data);
            break;
        case State::WriteHit:
            sig.cache_hit  = true;
            sig.cpu_stall  = true;
            break;
        case State::Miss:
            sig.cache_miss = true;
            sig.cpu_stall  = true;
            break;
        case State::CheckDirty:
            sig.cpu_stall  = true;
            sig.note       = victim_dirty_ ? "dirty=1" : "dirty=0";
            break;
        case State::WriteBack:
            sig.cpu_stall  = true;
            break;
        case State::MemWrite:
            sig.cpu_stall  = true;
            sig.mem_wr     = true;
            break;
        case State::MemWaitWrite1:
            sig.cpu_stall  = true;
            sig.mem_wr     = true;
            break;
        case State::MemWaitWrite2:
            sig.cpu_stall  = true;
            sig.mem_ready  = true;
            break;
        case State::Allocate:
            sig.cpu_stall  = true;
            break;
        case State::MemRead:
            sig.cpu_stall  = true;
            sig.mem_rd     = true;
            break;
        case State::MemWaitRead1:
            sig.cpu_stall  = true;
            sig.mem_rd     = true;
            break;
        case State::MemWaitRead2:
            sig.cpu_stall  = true;
            sig.mem_data_valid = true;
            break;
        case State::UpdateCacheBlock:
        case State::UpdateTag:
        case State::SetValid:
        case State::CheckWriteAfterAlloc:
        case State::WriteAfterAlloc:
        case State::UpdateCache:
        case State::SetDirty:
            sig.cpu_stall  = true;
            break;
        case State::Done:
            sig.cpu_stall  = false;
            if (pending_.has_value() && pending_->type == ReqType::Read)
            {
                sig.data_out_valid = true;
                sig.data_out = current_line_->data;
                sig.note     = "data=" + to_string(current_line_->data);
            }
            break;
        }

        // ── next-state logic + actions ──────────────────────────────
        switch (cur)
        {
        case State::Idle:
            if (incoming.has_value())
            {
                pending_ = incoming;
                active_text_ = req_to_str(*pending_);
                ++stats_.requests;
                ++(pending_->type == ReqType::Read ? stats_.reads : stats_.writes);
                state_ = State::CompareTag;
            }
            break;

        case State::CompareTag:
        {
            auto &r = pending_.value();
            cur_idx_  = index_of(r.addr);
            cur_tag_  = tag_of(r.addr);
            current_line_ = &lines_.at(cur_idx_);

            if (current_line_->valid && current_line_->tag == cur_tag_)
            {
                state_ = (r.type == ReqType::Read) ? State::ReadHit : State::WriteHit;
                ++stats_.hits;
            }
            else
            {
                victim_dirty_ = current_line_->valid && current_line_->dirty;
                victim_tag_   = current_line_->tag;
                victim_data_  = current_line_->data;
                state_ = State::Miss;
                ++stats_.misses;
            }
            break;
        }

        case State::ReadHit:
            state_ = State::Done;
            break;

        case State::WriteHit:
            state_ = State::UpdateCache;
            break;

        case State::Miss:
            state_ = State::CheckDirty;
            break;

        case State::CheckDirty:
            state_ = victim_dirty_ ? State::WriteBack : State::Allocate;
            break;

        // ── write-back path (2-cycle memory write) ──────────────────
        case State::WriteBack:
            state_ = State::MemWrite;
            ++stats_.writebacks;
            break;

        case State::MemWrite:
            state_ = State::MemWaitWrite1;
            break;

        case State::MemWaitWrite1:
            state_ = State::MemWaitWrite2;
            break;

        case State::MemWaitWrite2:
        {
            uint32_t victim_addr = rebuild_addr(victim_tag_, cur_idx_);
            mem.write_word(victim_addr, victim_data_);
            state_ = State::Allocate;
            break;
        }

        // ── allocate path (2-cycle memory read) ─────────────────────
        case State::Allocate:
            state_ = State::MemRead;
            break;

        case State::MemRead:
            state_ = State::MemWaitRead1;
            break;

        case State::MemWaitRead1:
            state_ = State::MemWaitRead2;
            break;

        case State::MemWaitRead2:
            fetched_data_ = mem.read_word(pending_->addr);
            state_ = State::UpdateCacheBlock;
            break;

        // ── install fetched block into cache ────────────────────────
        case State::UpdateCacheBlock:
            current_line_->data = fetched_data_;
            state_ = State::UpdateTag;
            break;

        case State::UpdateTag:
            current_line_->tag = cur_tag_;
            state_ = State::SetValid;
            break;

        case State::SetValid:
            current_line_->valid = true;
            current_line_->dirty = false;
            state_ = State::CheckWriteAfterAlloc;
            break;

        // ── post-allocation decision ────────────────────────────────
        case State::CheckWriteAfterAlloc:
            state_ = (pending_->type == ReqType::Write)
                         ? State::WriteAfterAlloc
                         : State::Done;
            break;

        case State::WriteAfterAlloc:
            state_ = State::UpdateCache;
            break;

        // ── shared write path (write-hit and write-after-alloc) ─────
        case State::UpdateCache:
            current_line_->data = pending_->data;
            state_ = State::SetDirty;
            break;

        case State::SetDirty:
            current_line_->dirty = true;
            state_ = State::Done;
            break;

        case State::Done:
            pending_.reset();
            state_ = State::Idle;
            break;
        }

        // ── build trace row ─────────────────────────────────────────
        TraceRow row;
        row.cycle        = cycle;
        row.state        = cur;       // the state we were IN this cycle
        row.sig          = sig;
        row.request_text = active_text_;
        stats_.cycles    = static_cast<uint64_t>(cycle);
        return row;
    }

    const vector<CacheLine> &cache_lines() const { return lines_; }
    const Stats             &stats()       const { return stats_; }

private:
    size_t   index_of(uint32_t a) const { return static_cast<size_t>(a % lines_.size()); }
    uint32_t tag_of  (uint32_t a) const { return a / static_cast<uint32_t>(lines_.size()); }
    uint32_t rebuild_addr(uint32_t tag, size_t idx) const
    {
        return tag * static_cast<uint32_t>(lines_.size()) + static_cast<uint32_t>(idx);
    }
    string req_to_str(const Request &r) const
    {
        stringstream ss;
        ss << req_name(r.type) << " " << hex32(r.addr);
        if (r.type == ReqType::Write) ss << " data=" << r.data;
        return ss.str();
    }

    vector<CacheLine>   lines_;
    State               state_{State::Idle};
    optional<Request>   pending_;
    string              active_text_;

    size_t              cur_idx_{0};
    uint32_t            cur_tag_{0};
    CacheLine          *current_line_{nullptr};

    bool                victim_dirty_{false};
    uint32_t            victim_tag_{0};
    uint32_t            victim_data_{0};
    uint32_t            fetched_data_{0};

    Stats               stats_;
};

// ── file I/O helpers ────────────────────────────────────────────────
bool ensure_dir(const string &p)
{
    fs::path parent = fs::path(p).parent_path();
    if (parent.empty()) return true;
    error_code ec; fs::create_directories(parent, ec);
    return !ec;
}

bool write_trace_csv(const string &path, const vector<TraceRow> &trace)
{
    if (!ensure_dir(path)) { cerr << "Error: dir for " << path << "\n"; return false; }
    ofstream out(path);
    if (!out) { cerr << "Error: open " << path << "\n"; return false; }

    out << "cycle,state,request,cpu_req_ready,cpu_stall,cache_hit,cache_miss,"
           "mem_rd,mem_wr,mem_ready,mem_data_valid,data_out_valid,data_out,note\n";
    for (auto &r : trace)
    {
        out << r.cycle << ',' << state_name(r.state) << ',' << '"' << r.request_text << '"' << ','
            << r.sig.cpu_req_ready << ',' << r.sig.cpu_stall << ','
            << r.sig.cache_hit << ',' << r.sig.cache_miss << ','
            << r.sig.mem_rd << ',' << r.sig.mem_wr << ','
            << r.sig.mem_ready << ',' << r.sig.mem_data_valid << ','
            << r.sig.data_out_valid << ',' << r.sig.data_out << ','
            << '"' << r.sig.note << '"' << '\n';
    }
    return true;
}

bool write_summary(const string &path, const vector<Request> &reqs,
                   const vector<CacheLine> &lines, const Stats &st, size_t nlines)
{
    if (!ensure_dir(path)) { cerr << "Error: dir for " << path << "\n"; return false; }
    ofstream out(path);
    if (!out) { cerr << "Error: open " << path << "\n"; return false; }

    out << "Cache Controller FSM Simulation Summary\n"
        << "=====================================\n"
        << "Cache lines: " << nlines << "\n"
        << "Memory latency: 2 cycles (fixed, per FSM diagram)\n\n"
        << "Requests loaded: " << reqs.size() << "\n"
        << "Total cycles: " << st.cycles << "\n"
        << "Reads: "  << st.reads  << "\n"
        << "Writes: " << st.writes << "\n"
        << "Hits: "   << st.hits   << "\n"
        << "Misses: " << st.misses << "\n"
        << "Writebacks: " << st.writebacks << "\n";

    uint64_t acc = st.hits + st.misses;
    if (acc > 0)
    {
        out << fixed << setprecision(2)
            << "Hit rate: " << (static_cast<double>(st.hits)*100.0/static_cast<double>(acc)) << "%\n";
    }

    out << "\nFinal cache state:\n";
    for (size_t i = 0; i < lines.size(); ++i)
        out << "Line " << i << ": valid=" << lines[i].valid
            << ", dirty=" << lines[i].dirty
            << ", tag=" << lines[i].tag
            << ", data=" << lines[i].data << '\n';
    return true;
}

// ── CLI ─────────────────────────────────────────────────────────────
void print_usage(const char *prog)
{
    cout << "Usage: " << prog << " [options]\n"
         << "  --input <path>       Input request file\n"
         << "  --trace <path>       Trace CSV output (default: outputs/trace.csv)\n"
         << "  --summary <path>     Summary output   (default: outputs/summary.txt)\n"
         << "  --cache-lines <N>    Number of cache lines (default: 4)\n"
         << "  --help               Show this help\n";
}

int main(int argc, char **argv)
{
    string input_path, trace_path = "outputs/trace.csv", summary_path = "outputs/summary.txt";
    size_t cache_lines = 4;

    for (int i = 1; i < argc; ++i)
    {
        string a = argv[i];
        if      (a == "--help")                          { print_usage(argv[0]); return 0; }
        else if (a == "--input"       && i+1 < argc)     input_path   = argv[++i];
        else if (a == "--trace"       && i+1 < argc)     trace_path   = argv[++i];
        else if (a == "--summary"     && i+1 < argc)     summary_path = argv[++i];
        else if (a == "--cache-lines" && i+1 < argc)     cache_lines  = static_cast<size_t>(stoull(argv[++i]));
        else { cerr << "Unknown arg: " << a << "\n"; print_usage(argv[0]); return 1; }
    }
    if (cache_lines == 0) { cerr << "--cache-lines must be > 0\n"; return 1; }

    auto requests = load_requests(input_path);
    MainMemory mem(64);
    CacheController ctrl(cache_lines);

    vector<TraceRow> trace;
    size_t ri = 0;
    int cycle = 0;

    // header
    cout << left
         << setw(6)  << "Cyc"
         << setw(26) << "State"
         << setw(22) << "Request"
         << setw(6)  << "Rdy"
         << setw(6)  << "Stall"
         << setw(5)  << "Hit"
         << setw(5)  << "Miss"
         << setw(5)  << "MRd"
         << setw(5)  << "MWr"
         << setw(5)  << "MRdy"
         << setw(6)  << "MDVld"
         << setw(6)  << "DOVld"
         << setw(10) << "DataOut"
         << "Note" << '\n'
         << string(120, '-') << '\n';

    while (ri < requests.size() || !ctrl.ready_for_request())
    {
        optional<Request> incoming;
        if (ri < requests.size() && ctrl.ready_for_request())
            incoming = requests[ri++];

        ++cycle;
        TraceRow row = ctrl.tick(cycle, incoming, mem);
        trace.push_back(row);

        cout << left
             << setw(6)  << row.cycle
             << setw(26) << state_name(row.state)
             << setw(22) << row.request_text
             << setw(6)  << row.sig.cpu_req_ready
             << setw(6)  << row.sig.cpu_stall
             << setw(5)  << row.sig.cache_hit
             << setw(5)  << row.sig.cache_miss
             << setw(5)  << row.sig.mem_rd
             << setw(5)  << row.sig.mem_wr
             << setw(5)  << row.sig.mem_ready
             << setw(6)  << row.sig.mem_data_valid
             << setw(6)  << row.sig.data_out_valid
             << setw(10) << (row.sig.data_out_valid ? to_string(static_cast<unsigned long long>(row.sig.data_out)) : "--")
             << row.sig.note << '\n';
    }

    bool ok = write_trace_csv(trace_path, trace)
            & write_summary(summary_path, requests, ctrl.cache_lines(), ctrl.stats(), cache_lines);

    cout << "\nTrace written to: "   << trace_path   << '\n'
         << "Summary written to: " << summary_path << '\n';
    return ok ? 0 : 2;
}
