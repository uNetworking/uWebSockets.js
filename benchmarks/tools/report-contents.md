## Benchmark Results

| (index) | Framework | Requests/sec | Transfer/sec |  Latency   | Total Requests | Transfer Total | Latency Stdev | Latency Max |
|---------|-----------|--------------|--------------|------------|----------------|----------------|---------------|-------------|
|    0    |   uws     |   34801.87   |    3.48MB    |  29.22ms   |     349784     |     35.03MB    |    2.53ms     |   72.43ms   |
|    1    |   http    |   24739.38   |    4.20MB    |  41.16ms   |     248928     |     42.26MB    |    7.45ms     |  185.33ms   |
|    2    |  fastify  |   24002.04   |    4.03MB    |  42.57ms   |     241340     |     40.51MB    |    8.77ms     |  249.95ms   |
|    3    |   koa     |   20874.8    |    3.48MB    |  48.37ms   |     210834     |     35.19MB    |   10.52ms     |  276.27ms   |
|    4    | restify   |   13881.54   |    2.45MB    |  73.55ms   |     139624     |     24.63MB    |   17.21ms     |  363.28ms   |
|    5    |   hapi    |   13473.97   |    2.84MB    |  75.93ms   |     135722     |     28.61MB    |   20.09ms     |  484.66ms   |
|    6    | express   |   6865.61    |    1.56MB    | 147.68ms   |     69122      |     15.69MB    |   29.83ms     |  740.75ms   |

