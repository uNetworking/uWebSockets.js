# Benchmark

A/B of two builds of this addon: the `dist` of `--head` over the `dist` of `--base`, scenario by
scenario. The CI runs it on every pull request with the base branch in `--base` and the PR in
`--head`, and posts the table as a comment on the PR (in the job summary when the PR comes from a
fork, where the token cannot comment). `workflow_dispatch` compares any branch against any ref.

Both checkouts must be built (`make`). The http rows are loaded with [wrk](https://github.com/wg/wrk),
the ws rows with `load_test` from the uWebSockets submodule, built as in the workflow:

```bash
cd uWebSockets/benchmarks
clang -O3 -DLIBUS_USE_OPENSSL -I../uSockets/src ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c load_test.c -c
clang++ -O3 -DLIBUS_USE_OPENSSL -I../uSockets/src ../uSockets/src/crypto/*.cpp -c -std=c++17
clang++ -O3 *.o -lssl -lcrypto -lz -o load_test
```

```bash
node benchmark/compare.js --base ../uWebSockets.js-master --head . --rounds 4 --duration 3
node benchmark/compare.js --base . --head .                  # same code on both sides: what it prints is noise
node benchmark/compare.js --base ../base --head . --scenario http/hello-world --rounds 9
```

Four servers stay up, two per arm, and the load alternates between them one scenario at a time,
swapping the order every round, so the two measurements behind a ratio are seconds apart and a
drift of the machine lands on both. The second process of each arm runs the same code as the
first: how far base/base and head/head get from 1.0 is the noise of that run, and a head/base
ratio is marked only when it moved further than that. Only the ratios are comparable across runs,
the absolute req/s depend on the machine.

Each server reports its own cpu time, so a row also says how busy the server was and what a
request cost it. When the server is well under 100% busy the load generator set the pace, the
req/s are its and the cpu per request is the column to read. On Linux the server is pinned to a
cpu whose hyperthread sibling stays idle and the load generators to the other cpus.

The scenarios are in `scenarios.js`: a hello world, a route that reads and writes headers, a JSON
POST read through `onData`, a microcached route answered natively, and a ws echo at 20 bytes and
at 4 KB.
