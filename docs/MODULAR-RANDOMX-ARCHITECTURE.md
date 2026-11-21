# Modular RandomX integration standard

**Status:** Approved project architecture / design standard (21 November 2026). Implementation and independent qualification remain pending. **Not a mainnet fork activation approval.**

**Project principle:** **Reliability first, release second.**

**Canonical home:** [Xnuva-RandomX-Engine](https://github.com/Xnuva-Blockchain-Project/Xnuva-RandomX-Engine). This document is the source of truth for integrating and maintaining the shared RandomX engine across the Xnuva Blockchain Project. The [coin integration checklist](COIN-INTEGRATION.md) governs each coin's consensus-specific release tests.

## Decision

Maintain **one independently versioned shared RandomX C++ library**, with **one small, explicitly audited adapter in each modernised coin**.

- No parallel copies of the algorithm maintained inside the TAO, Europa, Zeus and Hopium repositories.
- No implicit change to XNUVA's already deployed RandomX validation or existing live chain: treat XNUVA as a compatibility reference, not an automatic refactoring target.
- Every coin pins a **specific immutable library commit/release** and the exact RandomX variant/source revision, and records the pinned version in its build manifest and tests.
- Every library update is **opt-in per coin**, and proceeds through independent testing, review, reproducible builds and a separately approved release.
- Neither a GitHub tag/branch nor a successful compilation grants permission for a network hard fork.

The future public rebrand remains **EuropaX (ERAX), TaoX (TOAX), ZeusX (ZUSX), HopiumX (HOPEX)**; XNUVA stays XNUVA. This is a naming plan only. The existing historical coin identities remain authoritative until qualified network upgrades are announced.

## Architecture and responsibilities

```text
Xnuva-RandomX-Engine  (independent, versioned C++ module)
  ├─ Exact RandomX algorithm/variant and raw digest implementation
  ├─ Light-mode hashing and reusable cache/VM lifecycle
  ├─ Future optional verified fast-mode mining implementation
  ├─ Deterministic public vectors, parity/resource/portability tests
  └─ Published immutable release commits, reproducible build instructions
                      │
             exact pinned version
           ┌──────────┼────────────┬───────────────┐
           │          │            │               │
      TaoX adapter   EuropaX    ZeusX adapter  HopiumX adapter
       (TOAX)         (ERAX)       (ZUSX)          (HOPEX)
           │          │            │               │
      TAO consensus  ERA rules   ZUS PoW+PoS    HOPE PoW+PoS
```

**The library answers:** Given the exact algorithm variant, the input bytes and the seed key, what is the 32-byte raw RandomX digest? It manages hashing resources and exposes a narrow, deterministic interface.

**The coin adapter answers:** Does the specific candidate block need RandomX? How is its header serialised, what is its validated-chain seed, how is the digest compared to the compact target, and which difficulty, chainwork, reorg and fork rules apply? These are **consensus rules** and do **not** belong in the shared module.

The library must not depend on any coin's `CBlock`, `CBlockIndex`, chainparams, wallet, staking, RPC or mining rotation infrastructure. Coin adapters may depend on the library; the reverse dependency is forbidden.

## Integration is source/build-time, not hot-swapping

Treat the engine as a **build-time C++ dependency**, such as a pinned Git submodule, vendored dependency with an auditable source manifest, or CMake `FetchContent` using a full commit SHA. It is **not** a dynamically replaceable consensus plugin that a running node can upgrade without restart and validation.

An illustrative integration sketch (not a drop-in build recipe):

```cmake
# Ensure the source at vendor/Xnuva-RandomX-Engine is pinned and audited.
add_subdirectory(vendor/Xnuva-RandomX-Engine)
target_link_libraries(coin_node PRIVATE xnuva::randomx_foundation)
```

Pin the **entire** dependency graph, including the underlying XNUVA RandomX v2 implementation, compiler/toolchain requirements and (when applicable) upstream patches. No unpinned `main` branch, floating release tag or silently swapped system `librandomx` for consensus builds. Make offline/reproducible builds possible.

## Non-negotiable consensus boundaries

1. **No history rewrite:** Preserve each original genesis, all valid historical blocks/IDs/transactions, addresses, private keys, balances, existing monetary supply and wallet recovery.
2. **Explicit fork gate:** Until the independently specified and approved activation height/time, use the coin's **original** proof-of-work rules. At and after activation, use RandomX **only** where the coin's consensus specifies it. No fork heights or dates are set by this architecture.
3. **Distinct seed schedules:** Define each coin's seed/key derivation, epochs, anchor/block identity, canonical ancestor validation, header encoding, byte order, target comparison, initial post-fork difficulty, retargeting and chainwork. Do **not** assume XNUVA seed parameters automatically suit other coins.
4. **Hybrid preservation:** Zeus/ZUS and Hopium/HOPE are **Proof-of-Work plus Proof-of-Stake** networks. Preserve PoS kernels, stake modifiers, eligibility, timestamp/signature checks, staking rewards and PoS/hybrid chain selection across the future PoW transition.
5. **Algorithm compatibility:** XNUVA currently uses the specific `RANDOMX_FLAG_V2` algorithm variant. Generic upstream RandomX v1 is **not byte-for-byte interchangeable**; algorithm variants are explicit consensus choices.
6. **Block identifiers:** Do not assume the RandomX PoW result becomes the historical block ID. Document and test `GetHash()` / PoW hash separation where applicable.
7. **No wallet or treasury coupling:** No private keys, wallet balances, rewards, exchange functions or XNUVA Game treasury policy inside the RandomX engine.

## Library change classification

| Change type | Potential handling | Required proof |
|---|---|---|
| Optimisation, caching, JIT/portable path, memory/resource management | May be shipped as a normal software upgrade **only if results are strictly identical** for every valid consensus input on supported platforms | Independent deterministic vectors, adversarial inputs, seed-rollover/reorg tests, light/fast parity where available, cross-platform parity and sustained soak/restart checks |
| API/ABI, build-system, dependency or toolchain change | Rebuild and retest **each** consuming coin; may require a coordinated software release | Correct linkage, reproducibility, backward compatibility, full regression |
| Any change producing a different PoW digest or changing input/seed rules | **Consensus change**; not a drop-in library upgrade, and typically requires a separately specified and coordinated hard fork | Historical replay + fork-boundary tests, independent node acceptance/rejection, difficulty/chainwork/reorg proofs, full wallet and treasury qualification |

**Important:** The labels above guide engineering review; even an intended "performance-only" patch is not automatically consensus-neutral. A bug fix that corrects previously accepted hashes can itself be consensus-breaking.

## Versioning, review and rollback workflow

1. **Engine first:** Change code only on the dedicated engine development branch; link issue, immutable commit, algorithm variant, licence review, test vectors and any measured performance impact.
2. **Engine qualification:** Compile on Linux and Windows, run deterministic vectors, negative/error cases, light/fast equivalence (when implemented), portability, parallel/restart/cache-failure tests and independent reference checks. No PASS without actual execution evidence.
3. **Publish candidate:** Produce a signed or otherwise verifiable, immutable engine release/tag with source commit and dependency provenance, plus a changelog stating whether hashes/consensus are preserved.
4. **Choose an adopter:** Update **one coin's experimental modernisation branch** to the exact engine version. Keep its current lockfile/submodule commit and a known-good rollback checkpoint.
5. **Test coin adapter:** Prove existing chain history validates, activation boundaries `H-1/H/H+1` are correct, alternative branches/seed rollover work, mining/staking/rewards mature properly, restarts recover, and independent nodes agree.
6. **Operator coordination:** Explicitly approve and announce any separately required hard fork; validate wallet upgrade and security procedures, and give miners and node operators time to coordinate.
7. **Release/monitor:** Ship only the qualified coin build. Monitor independent peers, wallet rewards, treasury/indexer attribution and 24–48h sustained operation. Roll back an incompatible **software build** when safe; never claim an already activated consensus fork can simply be reversed.
8. **Repeat per coin:** TAO, ERA, ZUS, HOPE are separate qualification programmes. A successful upgrade of one is not proof for another.

## Current implementation status (21 November 2026)

- **GitHub source prepared:** Standalone `Xnuva-RandomX-Engine` contains C++20 `LightHasher`, `Hash80ByteHeader`, portable light-cache lifecycle, CMake and candidate offline vectors.
- **Pinned underlying source:** XNUVA commit `853a3171ece198f60b5125a16dca83d68a8f53c3`.
- **Not yet proven:** Standalone CMake/CTest actually building and passing, cross-platform reference parity, efficient fast-mode mining, memory/performance profile, source/licence audit or production dependency releases.
- **Coin adapters:** TAO has a separate experimental initial hashing adapter that is **not** yet integrated with this shared library; no coin has yet imported, linked, qualified or activated the standalone engine.
- **Existing live networks:** Must not be changed by publication of this document. The published EuropaZeus.org roadmap describes future intended names/forks, not completed upgrades.

## References

- [Engine README](../README.md)
- [Per-coin consensus adapter and release checklist](COIN-INTEGRATION.md)
- [RandomX programme and public rebranding roadmap](https://github.com/Xnuva-Blockchain-Project/Xnuva/issues/4)
- [Golden Standard Cryptocurrency Modernisation Handbook](https://github.com/Xnuva-Blockchain-Project/Xnuva/blob/docs/golden-standard-modernisation-handbook-v1/doc/engineering-standards/GOLDEN_STANDARD_CRYPTO_MODERNISATION.md)

**This architecture is binding engineering policy for the project, not permission to alter the operational chain.**
