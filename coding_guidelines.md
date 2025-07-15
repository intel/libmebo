# General Guidelines

## C Language Guidelines

| # | Standard | Explanation |
|---|----------|-------------|
| C.GG.1 | Implementations SHALL restrict the scope of variables to that which is necessary. This includes avoiding the use of global variables, especially those that have cross-module scope. | Variables with long-lived or cross-module scope introduce various problems, including increased test and analysis complexity as well as a reduced ability to make changes to code. |
| C.GG.2 | Implementations SHOULD use a modern linting tool like clang-tidy during code development to prevent common coding errors and to produce higher quality code. | clang-tidy is a customizable and extensible linting tool that will provide warnings regarding common coding errors. |
| C.GG.3 | Implementations SHALL surround #define directives with parentheses to avoid unexpected behavior (UB) when defining immutable values. | Corresponding CERT C Coding Standard rules provide more details PRE01-C PRE02-C. |

## Memory Management

| # | Standard | Explanation |
|---|----------|-------------|
| C.MM.1 | The return value of all dynamic memory allocations through APIs like malloc() SHALL be checked before dereferencing the returned pointer. | Calls to malloc() can fail and return NULL. Failure to check the return value may result in dereferencing a NULL pointer. |
| C.MM.2 | All pointers to dynamic memory SHALL be released through a call to free() as soon as the memory is no longer needed. | This prevents memory leaks and other defects. |
| C.MM.3 | All pointers to dynamic memory SHALL be assigned to NULL after the dynamic memory is freed. | Dereferencing a pointer after a call to free() is undefined behavior and may produce disastorous results. Setting pointers to NULL after free() prevents this as well as accidental double free() calls. |
| C.MM.4 | All pointers to memory passed as arguments to functions that are exposed to attacker influence (e.g., at the API layer) SHALL be checked for validity (e.g., not NULL) before dereferencing them. | Dereferencing an invalid pointer may lead to various security vulnerabilities. |
| C.MM.5 | All accesses to memory through pointers SHALL prevent out of bounds reads and writes. | |
| C.MM.6 | Implementations SHALL scrub (e.g., zeroize) sensitive data (e.g. cryptographic secrets) when it is no longer needed. | While calls like free() reclaim memory for future heap allocation, they do not scrub secrets from said memory, leaving the secrets exposed to attack. |
| C.MM.7 | Implementations SHALL prefer the use of safe buffer functions when they are available. | Functions like memcpy() and memmove() are error prone. C11 Addendum K defines safer versions of these functions and must be used when supported by the compiler. |
| C.MM.8 | Implementations that use dynamic memory SHOULD consider using a smart pointer library to prevent dynamic memory related defects. | Manual dynamic memory management in C is error prone. |

## Type Definition and Usage

| # | Standard | Explanation |
|---|----------|-------------|
| C.TY.1 | Implementations SHALL use the type system to constrain the value set of variables to those that are semantically valid. | Weakly typed variables are error prone and can lead to security vulnerabilities. This includes using signed integers when only unsigned integers make sense. |
| C.TY.2 | Implementations SHALL avoid the use of void* types. | void* variables are weakly typed and error prone. |
| C.TY.3 | Implementations SHALL use operator sizeof() to determine the size of a type and SHALL NOT hard code the size of a type. | The size of a type may vary due to platform or compiler specifics. Using sizeof() improves understandability and prevents errors. |
| C.TY.4 | Instances of structures SHALL NOT be tested for equality via bytewise comparison. | The value of structure padding bytes between fields is implementation specific. Byte-by-byte comparison can lead to incorrect comparison results. |
| C.TY.5 | When a specific integer width must be guaranteed, one of the fixed width data types SHALL be used. | Lengths of built in types such as int and long may differ between implementations. When a specific width must be guaranteed, use the types declared in <stdint.h> |
| C.TY.6 | Implementations SHALL only use signed char and unsigned char for numeric values represented in char types. | According to the C standard, char is neither a signed char nor an unsigned char, i.e., it is a distinct type whose representation is implementation defined and using it for numeric values may cause incorrect computational results. |
| C.TY.7 | char types SHALL be converted to unsigned char before being assigned or converted to a larger integer type. | Because the representation and behavior of char types is implementation defined, converting them to larger integer types may lead to issues. |
| C.TY.8 | Implementations SHALL NOT cast away const-ness. | Removing const exposes data to undesired modification. |
| C.TY.9 | Implementations SHALL prevent instances of implicit type coercion. | When type conversion is necessary, make it explicit through the use of casts. |
| C.TY.10 | Implementations SHOULD avoid declarations with more than two levels of pointer nesting. | The use of more than two levels of pointer nesting may impair the ability to understand the behaviour of the code and make it error prone. |
| C.TY.11 | Any variable that is used to represent the size of an object SHOULD be declared to be of type size_t or rsize_t. | |
| C.TY.12 | Implementations SHOULD prefer the use of const or enum variables to define constants, rather than #define based preprocessor macros. | Preprocessor macros are implemented by textual substitution and are error prone. |
| C.TY.13 | Implementations SHOULD prefer the use of inline functions rather than #define based preprocessor macros. | Preprocessor macros are implemented by textual substitution and are error prone. |

## Function Definition and Invocation

| # | Standard | Explanation |
|---|----------|-------------|
| C.FU.1 | Implementations SHALL check the return values of all function calls that may fail. | Functions return values for a reason, often to indicate success vs. failure. Rigorously checking return values prevents numerous errors and vulnerabilities. |
| C.FU.2 | Implementations that define a function with no parameters SHALL declare/define it with a void parameter specifier. | Defining a function with a void argument list differs from declaring it with no arguments because, in the latter case, the compiler will not check whether the function is called with parameters at all, and may lead to incorrect behavior. |
| C.FU.3 | Implementations SHALL declare pointer function arguments as "pointer to const" (e.g., const int* pn) when the data pointed to should not be changed by the function. | Using const prevents accidental changes to data. |
| C.FU.4 | Implementations SHOULD use const appropriately to prevent unintended modifications to pointers and the objects they point to. Specifically:- Use const to indicate that the object pointed to by the pointer should not be modified: const int *ptr or int const *ptr.- Use const to indicate that the pointer itself should not be modified: int *const ptr. | Using const appropriately helps prevent accidental changes to both the pointer address and the object it points to. However, clarity and readability of the code are paramount. The use of const should be consistent with common practices and idioms in the codebase. |

## Input Validation

| # | Standard | Explanation |
|---|----------|-------------|
| C.IV.1 | Implementations SHOULD use a regular expression or context-free grammar parser implementation for input validation. | Ad hoc input validation techniques are error prone and expose the implementation to vulnerabilities. Numerous libraries exist to support this. |

## Integer Arithmetic

| # | Standard | Explanation |
|---|----------|-------------|
| C.IA.1 | Implementations SHALL explicitly prevent integer overflow/wraparound. | Integer overflow/wraparound is undefined behavior and may produce incorrect or unexpected results. |
| C.IA.2 | Implementations SHALL be aware of integer conversion rules. | C integer conversion and promotion rules are complex and confusing. |
| C.IA.3 | Implementations SHALL avoid mixing unsigned and signed integer types in arithmetic expressions. | Implicit conversion between signed and unsigned integers may cause unexpected or incorrect results. |
| C.IA.4 | Implementations SHALL make judicious use of () in arithmetic expressions to explicitly enforce operator precedence and to enhance understandability. | While C does define operator precedence rules, using () in arithmetic expressions makes the intention of the expression clear and thus reduces the likelihood of error. |
| C.IA.5 | Implementations SHALL prevent incorrect results through implicit integer promotion and sign extension. | |
| C.IA.6 | Implementations SHALL prevent unintentional loss of value precision through type narrowing. | |

## String Handling

| # | Standard | Explanation |
|---|----------|-------------|
| C.ST.1 | Implementations SHALL guarantee that storage for strings has sufficient space for character data and the null terminator. | It is a common error to forget the string NULL terminator when calculating the amount of storage required for a string. |
| C.ST.2 | Implementations SHALL NOT pass a non-null terminated character sequence to a library function that expects a null terminated string. | C does not have a built in string type, so strings are represented as NULL terminated character arrays. String manipulation functions assume that all strings are NULL terminated. |
| C.ST.3 | Implementations SHOULD use safe string functions rather than their unsafe variants. | Functions such as strcpy_s() and strcat_s() enforce a size limit on the string operations and prevent out of bounds reads and writes. |
| C.ST.4 | Implementations SHALL NOT use the %n format specifier with functions like printf() | The use of this format specifier is generally not needed and is error prone. |
| C.ST.5 | Implementations SHALL prevent user input from being used as a format string. | |

# General Guidelines (GL)

| # | Standard | Explanation |
|---|----------|-------------|
| GL.GG.1 | Implementations SHALL seek to reduce attacker degrees of freedom in their attempts to exploit implementation errors. | This reduces the size of the attack surface. This takes multiple forms, including implementing rigorous input validation and using the type system to reduce the value set to that which is semantically correct. |
| GL.GG.2 | Implementations SHALL use a properly developed threat model to determine both the relevance of countermeasures as well as the required level of defense in depth. | Not all possible countermeasures can or should be implemented. Doing so can add unnecessary costs to development, implementation complexity, performance degradation, and increased defects. |
| GL.GG.3 | Implementations SHALL implement high branch coverage unit tests. | All code must be accompanied by a set of developer authored and maintained unit tests that are executed during development of the code. |
| GL.GG.4 | Implementations SHALL be aware of how compiler optimizations may undo countermeasures and prevent such undesired behavior. | Modern compilers are very intelligent with respect to emitting efficient code. However, sometimes, the most efficient code may counteract security mechanisms. |
| GL.GG.5 | All security sensitive code SHALL be easy to read, easy to understand, and easy to change. | This follows the guidance provided in The Pragmatic Programmer. |
| GL.GG.6 | Implementations SHALL provide meaningful comments that document assumptions, preconditions, postconditions, invariants and complex logic that is difficult to understand from the code itself. | Other people, including our future selves, rely on our code being understandable in order to keep it error free. |
| GL.GG.7 | Implementations SHALL prevent time of check / time of use related vulnerabilities. | Performing checks on time sensitive data such as digital certificate validity is necessary. However the state of such checks may change over time. Failure to perform timely checks (e.g., certificate expiration or revocation) before use can lead to vulnerabilities. |
| GL.GG.8 | Implementations SHOULD prefer methods that prevent defects to those that rely on defect detection and remediation. | It is always less costly to prevent defects than to detect and remove them. |
| GL.GG.9 | Implementations SHOULD prefer a coding style that is declarative and functional, rather than procedural. | Let the code communicate what is to be done above how it is to be done. Relegate the "how" to lower level constructions, especially those provided by libraries. |
| GL.GG.10 | Implementations SHOULD use a linting tool (e.g. clang-tidy, pylint, etc.) to find and remove defects during code development. | Linting tools automatically identify suspicious code and often recommend easy fixes. Most of these tools also support integration with your IDE/editor. |
| GL.GG.11 | Implementations SHOULD follow industry standard best practices coding guidelines. | Numerous industry standard coding guidelines exist for various languages (e.g. CERT C Secure Coding Standard, C++ Core Guidelines, Python PEP 8, etc.) and provide guidance for writing robust code that is easy to understand, test, and maintain. |
| GL.GG.12 | Implementations SHOULD use programming languages and a programming style that enable easy adoption of best practices. | Programming languages have matured over time. Modern programming languages and styles support writing more correct and maintainable code through the use of strict type systems, generic programming, functional idioms, and compile time evaluation. As an example, modern C++ provides numerous mechanisms to write code that is just as efficient as C code, but is less error prone. |

## Memory Management

| # | Standard | Explanation |
|---|----------|-------------|
| GL.MM.1 | Implementations SHALL scrub secrets and other sensitive data from memory after use. | Failure to scrub secrets from memory leaves sensitive data exposed to attackers. |
| GL.MM.2 | Implementations SHALL check the return value of all memory allocation functions (e.g. C++ and C# new()) before using the memory. | Memory allocation functions may fail. When they do, failure to check the return value or handle the exception may lead to incorrect results or security vulnerabilities. |

## Type Definition and Usage

| # | Standard | Explanation |
|---|----------|-------------|
| GL.TY.1 | Implementations SHOULD use or define the most restrictive type that represents the range of semantically correct values. | Proper use of a programming language's type system will prevent many programmer errors that lead to vulnerabilities. |
| GL.TY.2 | Implementations SHOULD prefer using language enforced (e.g. C++ const) immutable variables where possible. | The type of a variable should always represent the semantics of the intended value set. For read-only data, this includes making the variable's type read-only through the use of language syntax. |

## Function Definition and Invocation

| # | Standard | Explanation |
|---|----------|-------------|
| GL.FU.1 | The number of parameters in a function definition SHOULD NOT exceed 6. | Functions with large parameter sets are error prone for callers and implementers. In addition, when a function accepts too many parameters, this is often an indicator that the function should be refactored into multiple, smaller functions. |
| GL.FU.2 | Implementations SHOULD clearly distinguish between functions that are "pure" and those that mutate observable (e.g. global) state. | Pure functions behave like mathematical functions in that they always return the same value for the same inputs. Distinguishing between pure and mutating functions makes code easier to debug, analyze, and change. |

## Input Validation

| # | Standard | Explanation |
|---|----------|-------------|
| GL.IV.1 | All APIs SHALL implement robust input validation. | Input validation at the API level is the first line of defense against various attacks, including stack corruption related attacks. |
| GL.IV.2 | Implementation SHALL verify all input that may be attacker sourced or modified. | This includes data from other subsystems that is communicated via an insecure link, data from files, etc. |
| GL.IV.3 | All input validation capabilities SHOULD use regular expressions or other "language formal" approaches when defining input validation logic. | Manual input validation is error prone. Using regular expressions, parser expression grammars, and other "automated" approaches are easier to implement correctly, test, and maintain. |
| GL.IV.4 | All input validation logic SHALL be tested through automated fuzzing. | Fuzzers provide a well understood set of tools for testing the robustness of input validation logic. |

## Integer Arithmetic

| # | Standard | Explanation |
|---|----------|-------------|
| GL.IA.1 | Implementations SHALL correctly check for underflow and overflow before executing arithmetic expressions. | Integer overflow and underflow is undefined behavior in many languages and may result in incorrect and/or exploitable results. |

## String Handling

| # | Standard | Explanation |
|---|----------|-------------|
| GL.ST.1 | All implementations SHALL NOT store secrets in unprotected strings that are embedded in the binary. | String data stored in binaries is easily harvested by many utilities and reverse engineering tools. |

## File I/O

| # | Standard | Explanation |
|---|----------|-------------|
| GL.FI.1 | All file paths SHALL be canonicalized before use. | Before using any file path, the path SHALL be expanded to resolve all relative directories, symbolic links, etc. Failure to do so may lead to path traversal vulnerabilities. |
| GL.FI.2 | Implementations SHALL treat file contents in untrusted locations as untrusted user input. | Files stored in untrusted/uncontrolled locations may be under attacker control and need to be verified like any other user input. |
| GL.FI.3 | Implementations SHALL check the return value of all file I/O functions. | File I/O functions in most languages may fail. Failure to check the return value of these functions can lead to security vulnerabilities. |
| GL.FI.4 | Implementations SHALL NOT create temporary files containing sensitive information in shared locations. | |
| GL.FI.5 | Implementations SHALL restrict access to files to the user(s) under which the process is executing. | Use OS filesystem permissions to restrict file access to the minimal set of required users as well as the minimal set of file operations (e.g. read vs. write) required by the application. |
| GL.FI.6 | Files containing secrets and other confidential data (e.g. cryptographic keys) SHALL be protected with an Intel approved authenticated encryption mechanism. | Ad-hoc mechanisms, including security through obscurity are not sufficient to protect confidential data. Using an Intel approved authenticated encryption mechanism provides the appropriate level of protection. |
| GL.FI.7 | Files containing integrity sensitive data SHALL be protected with an approved digital signature or message authentication code (MAC) mechanism. | Files such as digital certificates, configuration files, executable and library binaries can be maliciously changed. Correctly using an approved integrity protection mechanism prevents such vulnerabilities. |
| GL.FI.8 | Files containing structured data SHOULD use standard encodings (e.g. XML, JSON, etc.) or type-length-value (TLV) definitions to assist in maintaining the integrity of file contents. | Enforcing structure on data reduces the likelihood that the data will be misinterpreted either accidentally or maliciously. |

## Network I/O

| # | Standard | Explanation |
|---|----------|-------------|
| GL.NI.1 | Implementations SHALL use an approved mechanism (e.g. TLS, SPDM, SIGMA) for establishing a secure communications channel. | Establishing a secure communications channel through ad-hoc mechanisms is likely to result in numerous vulnerabilities. |

## Logging and Diagnostics

| # | Standard | Explanation |
|---|----------|-------------|
| GL.IO.1 | Implementations SHALL NOT write secrets and other sensitive data to logs or other diagnostics capabilities. | While logging and other diagnostics are needed for good operational support, writing sensitive data to these files exposes this data to information leakage to attackers. |

## Side Channel Resistance

| # | Standard | Explanation |
|---|----------|-------------|
| GL.SC.1 | Implementations SHALL ensure that logical decisions based on properties of sensitive data (e.g. cryptographic keys) execute in constant time. | Numerous security exploits take advantage of information leakage through non-constant execution of logic. |
| GL.SC.2 | Implementations SHALL avoid using short circuiting constructions (e.g. logical && and \|\|) with sensitive data. | Short circuiting constructions may introduce timing attack vunlerabilities. |
| GL.SC.3 | Implementations SHALL ensure that code that processes sensitive data have consistent memory access patterns. | A lack of consistent memory access patterns in code may expose the implementation to various memory related attacks, including cache timing attacks. |
| GL.SC.4 | Implementations SHALL prevent information leakage through logging and other diagnostics. | Logging and diagnostics capabilities, while essential, can leak sensitive information to an attacker. |
| GL.SC.5 | Implementations SHOULD prefer branchless implementations of conditional logic. | Branchless implementations are less subject to things like timing attacks, and usually result in more efficient code. |

## Reverse Engineering Resistance

| # | Standard | Explanation |
|---|----------|-------------|
| GL.RE.1 | Implementations SHALL NOT store secrets (e.g. cryptographic keys) in binaries. | Many tools make it easy to reverse engineer binaries and extract data, including security sensitive data. |

## Cryptographic Implementation

| # | Standard | Explanation |
|---|----------|-------------|
| CS.CR.1 | Implementations SHALL comply with the Intel Applied Cryptography Guidelines. | Intel's cryptographic guidelines provide guidance on the selection of cryptographic algorithms, appropriate cryptographic parameters (e.g., key length), and key management. |