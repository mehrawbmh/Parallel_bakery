# 🍞 Multithreaded Bakery Simulation

A C++ simulation of a bakery using **POSIX threads (pthreads)**, **mutexes**, **condition variables**, and **semaphores** to coordinate bakers, customers, and an oven.

## 🚀 Features
- Simulates multiple **bakers**, **customers**, and an **oven**.
- Uses **threads** to model real-world parallel execution.
- Ensures **synchronization** with mutexes, condition variables, and semaphores.
- Handles **dynamic order processing** from multiple customers.

## 🛠️ Installation & Compilation
Ensure you have **g++** installed and compile with:
```sh
g++ -std=c++20 -pthread -o bakery chaos.cpp single_baker.cpp multi_baker.cpp
```

## 🏃‍♂️ Running the Simulation
Run the executable:
```sh
./bakery
```
Follow the prompts to input customer names and bread orders.

## 📂 Files Overview
- **chaos.cpp** → Manages the main bakery simulation with multithreading.
- **single_baker.cpp** → Implementation of a single baker handling orders.
- **multi_baker.cpp** → Manages multiple bakers.
- **sample.txt** → Sample input file for testing.

## 🔧 How It Works
1. Customers place orders with specific bakers.
2. Bakers receive and process the orders, preparing breads.
3. The oven bakes breads with a fixed time per batch.
4. Customers receive their baked goods when ready.

## 🎯 Learning Outcomes
This project is useful for learning:
✅ **Multithreading** with `pthread`  
✅ **Thread synchronization** using mutexes and semaphores  
✅ **Concurrency control** in a real-world scenario  

## 🤝 Contributing
Contributions are welcome! Feel free to open issues or submit PRs.

## 📜 License
MIT License – feel free to use and modify!


