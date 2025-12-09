# ISO-TP

> [!CAUTION]
> The author is not responsible for any damages done.
> This software is NOT considered safety critical,
> nor the author is competent enough to design such software.
> You have been warned!

This page is available in [DOXYGEN](https://furdog.github.io/iso_tp/) format
### WIP
The project is under early development

## Current status
- Under active development

## Project overview
> [!NOTE]
> Implemented based on the technical specifications outlined in
> ISO 15765-2. The specification is not included in this
> repository due to legal reasons. The standard might be changed
> (to newer version) any time soon.

This repository contains limited implementation of ISO-TP, specifically 
consecutive frame node listener (at the moment of creation).
The initial purpose was to filter and replace CAN-TP multiframe messages
in real time (Nissan Leaf CAN filtering software).

The design is hardware-agnostic, requiring an external adaptation layer for
hardware interaction.

**Key Features:**
  * **Following specs:** `ISO 15765-2` (at the moment of creation)
  * **Pure C:** Specifically ANSI(C89) standard,
		featuring linux kernel style formatting
  * **MISRA-C compliant:** Integration providen by cppcheck
			(100% compliance achieved for core library)
  * **Designed by rule of 10:** No recursion, dynamic memory allocations,
				callbacks, etc
  * **Deterministic:** Designed with constant time execution in mind
  * **Hardware agnostic:** Absolute ZERO hardware-dependend code
  * **Zero dependency:** No dependencies has been used except standart library
  * **Object oriented:** Though written on C, the project tries to use
			 handles and method-like functions
  * **Asynchronous:** Fully asynchronous API, zero delay
  * **Test driven:** Tests before implementation!
		     Developed by folowing TDD (Test Driven Design/Development)
  * **Single header:** Makes integration with other projects
		       super simple and seamless
  * **Documented:** It is not really well made yet, but it's on the priority!
  * **GitHub actions:** Automated checks and doxygen generation

**Problems:**
  * **Not certified for safe use:** Use it at own risk
  * **WIP:** Actively work in progress (not for production)

## License
```LICENSE
Copyright (c) 2025 furdog <https://github.com/furdog>

SPDX-License-Identifier: 0BSD
```
