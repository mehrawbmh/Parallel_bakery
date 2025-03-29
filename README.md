# 🍞 Multithreaded Bakery Simulation

A comprehensive C++ simulation of two bakery models (single-baker and multi-baker) using POSIX threads (pthreads), mutexes, condition variables, and semaphores to study parallel processing efficiency in a real-world scenario.

## 📖 Project Overview
This project simulates two bakery models as described in the PP-CA3-Fall03 assignment:
1. **Single-baker bakery (Takpaz)**: One baker serving one queue of customers
2. **Multi-baker bakery (Chandpaz)**: Multiple bakers (2 to N, where N is system cores) each with their own customer queue, sharing a common oven

The simulation compares performance metrics (average/standard deviation of order-to-delivery time) between both models under different conditions, including a "chaos mode" where customers compete for baker attention.

## 🚀 Key Features
- Thread-based simulation of bakers, customers, and shared resources
- Three synchronization models:
  - `single_baker.cpp`: Traditional producer-consumer with one baker
  - `multi_baker.cpp`: Multiple producer-consumer systems with shared oven
  - `chaos.cpp`: Competitive customer model without orderly queues
- Performance metrics collection for order processing times
- Configurable parameters:
  - Oven capacity (max breads = 10 × baker count)
  - Baking time (2 minutes per bread)
  - Maximum order size (15 breads per customer)

## 🛠️ Build & Execution

### Prerequisites
- Linux system
- g++ (supporting C++20)
- pthread library

### Compilation
```sh
g++ -std=c++20 -pthread -o bakery main.cpp single_baker.cpp multi_baker.cpp chaos.cpp
```

### Execution Modes
1. **Single-baker mode**:
   ```sh
   ./bakery single < input_single.txt
   ```
   Input format:
   - Line 1: Customer names (space-separated)
   - Line 2: Bread counts per customer (matching name order)

2. **Multi-baker mode**:
   ```sh
   ./bakery multi < input_multi.txt
   ```
   Input format:
   - Line 1: Number of bakers (N)
   - Next N×2 lines: For each baker:
     - Customer names (space-separated)
     - Bread counts per customer

3. **Chaos mode** (multi-baker with competitive customers):
   ```sh
   ./bakery chaos < input_multi.txt
   ```

## 📊 Performance Analysis
The program outputs:
- Average order-to-delivery time per bread
- Standard deviation of delivery times
- Comparison between orderly queue vs. chaos mode
- Scaling analysis with varying baker counts

## 📂 File Structure
```
.
├── main.cpp            # Program entry point and mode selection
├── single_baker.cpp    # Single-baker implementation
├── multi_baker.cpp     # Ordered multi-baker implementation
├── chaos.cpp           # Competitive customer implementation
├── bakery.h            # Shared definitions and structures
├── Makefile            # Build automation
├── input_single.txt    # Sample single-baker input
├── input_multi.txt     # Sample multi-baker input
└── README.md           # This documentation
```

## 🔧 Implementation Details
- Synchronization primitives:
  - Mutexes for oven and delivery space access
  - Condition variables for baker-customer notification
  - Semaphores for oven capacity management
- Timer handling using `timer_create()` and `timer_settime()`
- Signal-safe global variables for timer callbacks
- Thread-safe data structures for order tracking

## 🎯 Learning Outcomes
This project demonstrates:
- POSIX thread management
- Advanced synchronization with mutexes and condition variables
- Resource contention in shared systems (oven)
- Performance analysis of parallel systems
- Real-world system modeling with producer-consumer patterns

## 📜 Sample Inputs
Example files are provided showing the required input formats for both single-baker and multi-baker modes.

## 🤝 Contributing
Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

Report issues through GitHub Issues.

## 📜 License
MIT License - see LICENSE file for details.

---

*Note: This implementation was developed based on the PP-CA3-Fall03 assignment requirements from the Parallel Programming course (Fall 1402/2023).*
