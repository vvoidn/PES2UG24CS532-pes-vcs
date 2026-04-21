# Mini Version Control System (PES-VCS)

## Overview

This project implements a simplified version of a Git-like version control system. It supports storing file contents as objects, building directory trees, maintaining a staging area (index), and creating commits.

---

## Features

* **Blob Storage**
  Stores file contents using SHA-256 hashing.

* **Tree Structure**
  Represents directory hierarchy using tree objects.

* **Index (Staging Area)**
  Tracks files added before committing.

* **Commit System**
  Creates snapshots of the project state with metadata.

---

## Project Structure

```id="dm65zl"
.
├── object.c / object.h     # Blob storage
├── tree.c / tree.h         # Tree (directory structure)
├── index.c / index.h       # Staging area
├── commit.c / commit.h     # Commit logic
├── pes.c                   # CLI interface
├── test_objects.c          # Phase 1 tests
├── test_tree.c             # Phase 2 tests
└── Makefile                # Build automation (if provided)
```

---

## Compilation

Compile the full system using:

```id="4vd8l0"
gcc -Wall -Wextra -O2 pes.c object.c tree.c index.c commit.c -lcrypto -o pes
```

---

## Usage

### Initialize repository

```id="kc7qmu"
./pes init
```

### Create a file

```id="varsbc"
echo "hello" > a.txt
```

### Add file to index

```id="46zorj"
./pes add a.txt
```

### Check status

```id="jv2awm"
./pes status
```

### Commit changes

```id="jgfdhy"
./pes commit -m "Initial commit"
```

---

## Sample Output

### Status

```id="3gkwhk"
a.txt
```

### Commit

```id="toha7e"
commit <hash>
Author: PES2UG24CS532
Date: <timestamp>

Initial commit
```

---

## Testing

### Phase 1: Object Tests

```id="d8kpl6"
gcc test_objects.c object.c -lcrypto -o test_objects
./test_objects
```

### Phase 2: Tree Tests

```id="3nj87a"
gcc test_tree.c tree.c object.c index.c -lcrypto -o test_tree
./test_tree
```

---

## Concepts Used

* SHA-256 hashing (OpenSSL)
* File handling in C
* Data structures (trees and arrays)
* Content-addressable storage
* Basic version control concepts

---

## Author

**Name:** Suhas Prusty
**SRN:** PES2UG24CS532

---

## Notes

* All objects are stored inside `.pes/objects/`
* The index is stored in `.pes/index`
* Each commit represents a snapshot of the project

---

## Status

Object storage implemented
Tree construction implemented
Index management implemented
Commit system implemented

---
