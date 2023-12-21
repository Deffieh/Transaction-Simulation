# Transaction-Simulation - Operating systems project

## What is that?

Concurrent Programming and Memory Management in a UNIX Environment: A University Project on Process Synchronization.

### Prerequisites

* A PC (preferably running Linux)
* C compiler (gcc)
* and basic knowledge of C to understand the code.

Note: In some instances, references to 'users' pertain to the processes simulating them.

### About

The objective of this project is to simulate a ledger containing data on monetary transactions among different users. The simulation involves the following processes:

- A **master** process responsible for overseeing the simulation, managing the creation of other processes, and more.
- SO_USERS_NUM **user** processes that can initiate transactions by sending money to other users.
- SO_NODES_NUM **node** processes that process transactions received, charging a fee for their services."


#### Users
**User Process Operations:**
1. **Balance Calculation:**
   - Calculate the current balance based on the initial budget, summing income and expenses in ledger transactions.
   - Subtract the amounts of transactions sent but not yet recorded.
   - If the balance is ≥ 2:
     - Randomly select:
       - Another user process as the recipient.
       - A node for transaction processing.
       - An integer value between 2 and the balance.
       - Calculate transaction reward (SO_REWARD% of the value, rounded, with a minimum of 1).
       - Transaction amount equals the value minus the reward.
     - Example: With a balance of 100, randomly selecting 50 (SO_REWARD = 20) results in transferring 40 to the recipient and 10 to the processing node.
   - If the balance is < 2, no transactions are sent.

2. **Transaction Sending:**
   - Send the transaction to the selected node.
   - Wait for a random time interval (in nanoseconds) between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC.
   - Generate a transaction in response to a received signal.

**Termination:**
- If unable to send transactions for SO_RETRY consecutive attempts, terminate execution, notifying the master process.


#### Nodes

- Each node privately stores a list of received transactions to process, called the transaction pool (can contain up to SO_TP_SIZE transactions, with SO_TP_SIZE > SO_BLOCK_SIZE).
- If the node's transaction pool is full, any additional transactions are discarded and not executed. In such cases, the sender of the discarded transaction must be informed.

**Transaction Processing:**

- Transactions are processed in blocks, with each block containing exactly SO_BLOCK_SIZE transactions.
  - SO_BLOCK_SIZE−1 transactions are received from users.
  - One transaction is a payment transaction for processing (see below).

**Node Lifecycle:**

1. **Creation of a Candidate Block:**
   - Extract SO_BLOCK_SIZE−1 transactions from the transaction pool that are not yet in the ledger.
   - Add a reward transaction to the block with the following characteristics:
     - Timestamp: current value of `clock_gettime(...)`
     - Sender: -1 (define a MACRO...)
     - Receiver: Identifier of the current node
     - Quantity: Sum of all rewards from transactions in the block
     - Reward: 0

2. **Simulation of Block Processing:**
   - Simulate block processing by non-actively waiting for a random time interval expressed in nanoseconds, ranging between SO_MIN_TRANS_PROC_NSEC and SO_MAX_TRANS_PROC_NSEC.

3. **Completion of Block Processing:**
   - After completing the block processing, write the newly processed block to the ledger.
   - Remove successfully executed transactions from the transaction pool.


## Node Initialization by Master Process

- Upon creation by the master process, each node receives a list of SO_NUM_FRIENDS friend nodes.

**Enhanced Node Lifecycle:**

1. **Periodic Transaction Sharing:**
   - Periodically, each node selects a transaction from the transaction pool that is not yet in the ledger.
   - Sends the selected transaction to a randomly chosen friend node. (The transaction is removed from the source node's transaction pool)

2. **Handling Full Transaction Pool:**
   - If a node receives a transaction but has a full transaction pool:
     - Sends the transaction to a randomly chosen friend node.
     - If the transaction doesn't find placement within SO_HOPS, the last receiving node sends it to the master process.
     - The master process creates a new node containing the discarded transaction as the first element in the transaction pool.
     - Assigns SO_NUM_FRIENDS randomly chosen friend nodes to the new node.
     - Randomly selects SO_NUM_FRIENDS existing nodes, instructing them to add the newly created node to their friend list.


## Termination

The simulation will terminate under one of the following conditions:
- SO_SIM_SEC seconds have elapsed.
- The ledger's capacity is exhausted (the ledger can contain up to SO_REGISTRY_SIZE blocks).
- All user processes have terminated.

Upon termination, the master process mandates all node and user processes to terminate and prints a simulation summary, including at least the following information:
- Reason for simulation termination.
- Balance of each user process, including those that terminated prematurely.
- Balance of each node process.
- Number of prematurely terminated user processes.
- Number of blocks in the ledger.
- For each node process, the number of transactions still present in the transaction pool.

## Configuration

**Runtime Parameters from conf/data.conf:**
- **SO_USERS_NUM:** Number of user processes.
- **SO_NODES_NUM:** Number of node processes.
- **SO_BUDGET_INIT:** Initial budget for each user process.
- **SO_REWARD:** Percentage of reward paid by each user for transaction processing.
- **SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC:** Minimum and maximum time (in nanoseconds) between the generation of consecutive transactions by a user.
- **SO_RETRY:** Maximum number of consecutive failures in transaction generation before a user process terminates.
- **SO_TP_SIZE:** Maximum number of transactions in the transaction pool of node processes.
- **SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC:** Minimum and maximum simulated processing time (in nanoseconds) for a block by a node.
- **SO_SIM_SEC:** Duration of the simulation (in seconds).
- **SO_NUM_FRIENDS (max 30, full version only):** Number of friend nodes for each node process.
- **SO_HOPS (max 30, full version only):** Maximum number of hops for a transaction towards friend nodes before the master creates a new node.

## Compilation and Execution Options

- **Compilation:**
  - `gcc -std=c89 -pedantic`
    - Note: `_GNU_SOURCE` is not required in the compilation command as it has been defined in `header.h`.

- **Makefile Usage:**
  - To compile: `make all`
  - To run: `make run`


## Credits
Project made by: @Deffieh @ @Natfl1x

**Thanks for visit, have fun!**
