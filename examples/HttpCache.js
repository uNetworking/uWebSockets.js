/* An example showing microcaching for /prices/gold and /prices/bitcoin */

const uWS = require('../dist/uws.js');
const port = 9001;

uWS.App().get('/prices/gold', async (res, req) => {

  console.log("Hitting JavaScript");
  console.time("cache update");

  /* Before going async we need to listen to socket abortions */
  res.onAborted(() => {
    res.aborted = true;
  });
  /* This would be some async DB action or fetch quest that you want to essentially rate-limit */
  const bitcoinPrice = await getGoldPriceJSON();
  if (!res.aborted) {
    res.cork(() => {
      console.log("JavaScript is done fetching async data");
      console.timeEnd("cache update");
      res.end(bitcoinPrice);
    });
  }

}, {
  /* This object specifies the caching options */
  lowerExpiry: 1,
  upperExpiry: 5
}).get('/prices/bitcoin', async (res, req) => {

  console.log("Hitting JavaScript");
  console.time("cache update");

  /* Before going async we need to listen to socket abortions */
  res.onAborted(() => {
    res.aborted = true;
  });
  /* This would be some async DB action or fetch quest that you want to essentially rate-limit */
  const bitcoinPrice = await getBitcoinPriceJSON();
  if (!res.aborted) {
    res.cork(() => {
      console.log("JavaScript is done fetching async data");
      console.timeEnd("cache update");
      res.end(bitcoinPrice);
    });
  }

}, {
  /* This object specifies the caching options */
  lowerExpiry: 1,
  upperExpiry: 5
}).listen(port, (token) => {
  if (token) {
    console.log('Listening to port ' + port);
  } else {
    console.log('Failed to listen to port ' + port);
  }
});

/**
 * Async getter that fetches real-time Bitcoin (BTC/USDT) price from Binance.
 * @returns {Promise<string>} Pretty-printed JSON payload.
 */
async function getBitcoinPriceJSON() {
  const url = 'https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT';

  try {
    const response = await fetch(url, {
      headers: {
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Pragma': 'no-cache'
      }
    });

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    const data = await response.json();
    const now = new Date();

    const output = {
      status: 'success',
      asset: 'Bitcoin',
      symbol: data.symbol,
      price: parseFloat(data.price),
      currency: 'USDT',
      timestamp: Math.floor(now.getTime() / 1000),
      isoDate: now.toISOString()
    };

    return JSON.stringify(output, null, 2);

  } catch (error) {
    return JSON.stringify({
      status: 'error',
      message: error.message
    }, null, 2);
  }
}

/**
 * Async getter to retrieve current Gold price without third-party packages.
 * @returns {Promise<string>} Pretty-printed JSON string with price metadata.
 */
async function getGoldPriceJSON() {
  const url = 'https://query1.finance.yahoo.com/v8/finance/chart/GC=F?interval=1m&range=1d';
  
  try {
    // Native fetch with a standard User-Agent header to avoid basic scraping blocks
    const response = await fetch(url, {
      headers: { 'User-Agent': 'Mozilla/5.0' }
    });

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    const payload = await response.json();
    const meta = payload?.chart?.result?.[0]?.meta;

    if (!meta || typeof meta.regularMarketPrice !== 'number') {
      throw new Error('Invalid or incomplete payload received from API.');
    }

    const output = {
      status: 'success',
      asset: 'Gold Futures',
      symbol: meta.symbol || 'GC=F',
      price: meta.regularMarketPrice,
      currency: meta.currency || 'USD',
      timestamp: meta.regularMarketTime,
      isoDate: new Date(meta.regularMarketTime * 1000).toISOString()
    };

    return JSON.stringify(output, null, 2);

  } catch (error) {
    return JSON.stringify({
      status: 'error',
      message: error.message
    }, null, 2);
  }
}
