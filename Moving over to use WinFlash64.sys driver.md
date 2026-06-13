# Technical Analysis: Direct Integration with WinFlash64.sys (Option B)

This document outlines the architectural changes required if the `Blazestrike` project is modified to communicate directly with the signed `WinFlash64.sys` driver instead of manual mapping `BlazeDriver.sys` (Option A).

---

## Overview of Option B

In this model, you do not run or map any custom unsigned kernel code (`BlazeDriver.sys`). Instead, the signed `WinFlash64.sys` remains loaded in the kernel, and the user-mode application (`Blazestrike`) communicates directly with it to perform read/write operations using its exposed physical memory mapping functionality.

```
+------------------+                   +--------------------+
|   Blazestrike    | --[IOCTL 222000]->|   WinFlash64.sys   |
| (User-Mode App)  |                   |   (Signed Driver)  |
+------------------+                   +--------------------+
         |                                       |
         | (Translates Virtual Addresses)        | (Maps Physical Pages)
         v                                       v
[Software Page Walker]                   [Physical Memory]
```

---

## Required Changes to Blazestrike

To pivot `Blazestrike` to this model, the backend memory communication layer in the `MemoryDriver` class must be redesigned.

### 1. Device Path and Handle Management
`Blazestrike` currently opens a handle to `\\\\.\\BlazeDriver`. You must update this to open the symbolic link exposed by `WinFlash64.sys`.

* **File to Modify:** `memory/shared.h` and `memory/memory_driver.h`
* **Change:** Replace `DRIVER_USER_PATH` with the target symbolic link for `WinFlash64.sys` (e.g., `\\\\.\\WinFlash` or the corresponding device link registered by the driver).

### 2. IOCTL Codes and Structure Alignment
The driver dispatcher in `WinFlash64.sys` (`sub_11390`) processes commands using custom IOCTLs. You must replace the `BlazeDriver` IOCTLs with the raw control codes expected by `WinFlash64.sys`.

* **Physical Memory Read IOCTL:** `0x222000`
* **Input Buffer Expectations:**
  The driver expects the system buffer (`MasterIrp`) to contain:
  1. The **Physical Address** (passed in the initial fields of the buffer).
  2. The **Length** of the mapping request (passed in the stack location parameter).

### 3. Virtual-to-Physical Address Translation (Software Page Walking)
Unlike `BlazeDriver.sys`, which uses `MmCopyVirtualMemory` to handle virtual addresses automatically, `WinFlash64.sys` uses `MmMapIoSpace`. This function requires **physical addresses**.

Because game data (offsets, entities) is represented as virtual addresses within the game process, `Blazestrike` must perform translation in user-mode:

1. **Retrieve the Directory Table Base (CR3):**
   You need the CR3 register value of the target game process (`cs2.exe`). This value points to the physical address of the Page Map Level 4 (PML4) table.
2. **Implement Page Table Walking:**
   For every memory read, `Blazestrike` must parse the virtual address and manually traverse the page tables:
   * **PML4** $\rightarrow$ **PDPT** (Page Directory Pointer Table) $\rightarrow$ **PD** (Page Directory) $\rightarrow$ **PT** (Page Table) $\rightarrow$ **Physical Frame**.
3. **Query via IOCTL:**
   Once the physical address is calculated, it is sent to `WinFlash64.sys` via IOCTL `0x222000` to copy the data.

---

## Stealth and Detection Trade-offs

| Vector | Option A (Manual Mapping) | Option B (Direct Vulnerable Driver) |
| :--- | :--- | :--- |
| **Unsigned Code** | High Risk (Unbacked kernel memory pages can be flagged by NMI scans). | **No Risk** (No unsigned code is resident in the kernel). |
| **Driver Presence** | Low Risk (Vulnerable loader driver is immediately unloaded). | **High Risk** (Known vulnerable driver must remain loaded, exposing it to hash/name blocklists). |
| **Handles & Devices** | High Risk (Requires creating a device object dynamically, which is anomalous). | Medium Risk (Requires maintaining an open handle to the signed driver's device). |
| **Communication** | Highly Monitored (Standard IOCTL calls to anomalous devices). | Monitored (IOCTL calls to a legitimate but vulnerable driver). |
