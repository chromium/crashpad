# Crashpad Overview Design

Status: Draft
Authors: siggi@chromium.org, mark@chromium.org
Last Updated: 2017-02-17

[TOC]

## Objective

Crashpad is a library for capturing, storing and transmitting post-mortem crash reports from a client to an upstream collection server. Crashpad aims to make it possible for clients to capture process state at the time of crash with the best possible fidelity and coverage, with the minimum of fuss.

Crashpad also provides a facility for clients to capture dumps of process state on-demand for diagnostic purposes.

Crashpad additionally provides minimal facilities for clients to adorn their crashes with application-specific metadata in the form of per-process key/value pairs. More sophisticated clients are able to adorn crash reports further through extensibility points that allow the embedder to augment the crash report with application-specific metadata.

## Background

It’s an unfortunate truth that any large piece of software will contain bugs that will cause it to occasionally crash. Even in the absence of bugs, software incompatibilities can cause program instability.

Fixing bugs and incompatibilities in client software that ships to millions of users around the world is a daunting task. User reports and manual reproduction of crashes can work, but even given a user report, often times the problem is not readily reproducible. This is for various reasons, such as e.g. system version or third-party software incompatibility, or the problem can happen due to a race of some sort. Users are also unlikely to report problems they encounter, and user reports are often of poor quality, as unfortunately most users don’t have experience with making good bug reports.

Automatic crash telemetry has been the best solution to the problem so far, as this relieves the burden of manual reporting from users, while capturing the hardware and software state at the time of crash.

Crash telemetry involves capturing post-mortem crash dumps and transmitting them to a backend collection server. On the server they can be stackwalked and symbolized, and evaluated and aggregated in various ways. Stackwalking and symbolizing the reports on an upstream server has several benefits over performing these tasks on the client. High-fidelity stackwalking requires access to bulky unwind data, and it may be desirable to not ship this to end users out of concern for the application size. The process of symbolization requires access to debugging symbols, which can be quite large, and the symbolization process can consume considerable other resources. Transmitting un-stackwalked and un-symbolized post-mortem dumps to the collection server also allows deep analysis of individual dumps, which is often necessary to resolve the bug causing the crash.

Transmitting reports to the collection server allows aggregating crashes by cause, which in turn allows assessing the importance of different crashes in terms of the occurrence rate and e.g. the potential security impact.

A post-mortem crash dump must contain the program state at the time of crash with sufficient fidelity to allow diagnosing and fixing the problem. As the full program state is usually too large to transmit to an upstream server, the post-mortem dump captures a heuristic subset of the full state.

The crashed program is in an indeterminate state and, in fact, has often crashed because of corrupt global state - such as heap. It’s therefore important to generate crash reports with as little execution in the crashed process as possible. Different operating systems vary in the facilities they provide for this.

## Overview

Crashpad is a client-side library that focuses on capturing machine and program state in a post-mortem crash report, and transmitting this report to a backend server - a “collection server”. The Crashpad library is embedded by the client application.
Conceptually, Crashpad breaks down into the handler and the client. The handler runs in a separate process from the client or clients. It is responsible for snapshotting the crashing client process’ state on a crash, saving it to a crash dump, and transmitting the crash dump to an upstream server.
Clients register with the handler to allow it to capture and upload their crashes.