# PL/S -- History

PL/S (Programming Language/Systems) is a proprietary, machine-oriented
systems programming language developed by IBM in the late 1960s.
Originally called BSL (Basic Systems Language), it was designed as a
"dialect" of PL/I tailored specifically for writing operating systems
and low-level software for IBM mainframes like the System/360 and
System/370.

## Core Architecture & Design

- **The PL/I Foundation:** PL/S leveraged the high-level readability,
  structure, and syntax of PL/I but stripped away elements unsuitable
  for low-level kernels.
- **No Runtime Library:** Standard PL/I requires massive runtime
  environment libraries to execute. Because you cannot use a runtime
  library while building the foundational layers of an operating
  system, PL/S was built to run stand-alone without any underlying
  software dependencies.
- **Assembler Source Output:** Unlike typical compilers that generate
  machine object code directly, the PL/S compiler translated code
  directly into System/370 Assembler source code. This output was then
  fed into a standard assembler to build the final binary, allowing
  for extreme optimization.
- **Inline Assembly & Register Control:** It included special
  constructs like `@GEN[ERATE]` and `@ENDGEN` that allowed developers
  to seamlessly inject raw assembly code and manipulate hardware
  registers directly within high-level code structures.

## Key Historical & Strategic Impact

- **Building the Mainframe Ecosystem:** PL/S was the primary language
  used to build IBM's flagship operating systems, including MVS (the
  ancestor of modern z/OS) and its Time Sharing Option (TSO). It was
  also used to develop System R, the world's first relational SQL
  database layer.
- **The "Secret" Language:** IBM kept the PL/S compiler strictly
  proprietary and internal-only, never officially releasing it to the
  public or standard customers. By withholding the compiler, IBM
  effectively prevented competitors (such as Amdahl, Fujitsu, and
  Hitachi) from easily modifying IBM's operating system source-level
  code.
- **Evolution:** Over the decades, PL/S evolved into several successive
  internal dialects, including PL/S II, PL/S III, PL/AS (Programming
  Language/Advanced Systems), and PL/X, which IBM systems developers
  still use today.
