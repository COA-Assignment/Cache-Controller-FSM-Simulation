CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
TARGET = cache_sim

all: $(TARGET)

$(TARGET): cache_sim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

run-full: $(TARGET)
	./$(TARGET) --input inputs/workload_full.txt --trace outputs/reference/full_trace.csv --summary outputs/reference/full_summary.txt

run-hits: $(TARGET)
	./$(TARGET) --input inputs/workload_hits.txt --trace outputs/reference/hits_trace.csv --summary outputs/reference/hits_summary.txt

run-writeback: $(TARGET)
	./$(TARGET) --input inputs/workload_writeback.txt --trace outputs/reference/writeback_trace.csv --summary outputs/reference/writeback_summary.txt

run-conflict: $(TARGET)
	./$(TARGET) --input inputs/workload_conflict.txt --trace outputs/reference/conflict_trace.csv --summary outputs/reference/conflict_summary.txt

run-all: run-full run-hits run-writeback run-conflict

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all run run-full run-hits run-writeback run-conflict run-all clean