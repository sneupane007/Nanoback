import yfinance as yf
import pandas as pd

def download_trading_data(ticker, interval, period, filename):
    """
    Downloads historical market data and saves it to a CSV.
    
    Parameters:
    -----------
    ticker : str
        The stock/crypto symbol (e.g., 'AAPL', 'BTC-USD', 'EURUSD=X').
    interval : str
        The data resolution. Options: '1m', '2m', '5m', '15m', '30m', '60m', '90m', '1h', '1d', '5d', '1wk', '1mo', '3mo'.
        Note: 1m data is only available for the last 7 days; 1h is available for the last 730 days.
    period : str
        The total time range to download. Options: '1d', '5d', '1mo', '3mo', '6mo', '1y', '2y', '5y', '10y', 'ytd', 'max'.
    filename : str
        The name of the output file (e.g., 'market_data.csv').
    """
    
    # Download data
    data = yf.download(tickers=ticker, period=period, interval=interval,
                       auto_adjust=True)

    if data.empty:
        print("No data found. Check your ticker symbol or parameters.")
        return

    # yfinance >= 0.2 returns a MultiIndex column header even for a single
    # ticker, e.g. ('Open', 'AAPL'). Left unflattened, to_csv writes a phantom
    # second header row (",AAPL,AAPL,...") that the C++ DataHandler parses as a
    # garbage all-zeros first bar. Drop the ticker level so the columns are flat.
    if isinstance(data.columns, pd.MultiIndex):
        data.columns = data.columns.get_level_values(0)

    # Ensure the format is: Date, Open, High, Low, Close, Volume
    data = data[['Open', 'High', 'Low', 'Close', 'Volume']]
    
    # Reset index to make 'Date' a column
    data.reset_index(inplace=True)
    
    # Rename the first column to 'Date' (yfinance uses 'Datetime' for intraday)
    data.rename(columns={data.columns[0]: 'Date'}, inplace=True)

    # Save to CSV
    data.to_csv(filename, index=False)
    print(f"Success! Data saved to {filename}")
    
if __name__ == "__main__":
    # --- CONFIGURATION AREA ---
    # Update these variables to change your data output
    SYMBOL = "AAPL"         # Ticker symbol (Yahoo Finance format)
    TIMEFRAME = "1d"        # Daily bars — full price history, real trends/reversals
    RETRIEVAL_RANGE = "max" # Entire available history (decades of daily bars)
    FILE_NAME = "AAPL.csv"
    # Note: intraday intervals are range-capped by Yahoo (1m -> last 7 days,
    # 1h -> last 730 days). For intraday strategies (scalping/orb/vwap), use
    # e.g. TIMEFRAME="1h", RETRIEVAL_RANGE="2y".

    
    
    download_trading_data(SYMBOL, TIMEFRAME, RETRIEVAL_RANGE, FILE_NAME)