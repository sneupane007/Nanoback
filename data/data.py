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
    data = yf.download(tickers=ticker, period=period, interval=interval)
    
    if data.empty:
        print("No data found. Check your ticker symbol or parameters.")
        return

    # yfinance returns a MultiIndex if multiple tickers are used; 
    # we flatten it and ensure the format is: Date, Open, High, Low, Close, Volume
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
    SYMBOL = "NVDA"      # Ticker symbol (Yahoo Finance format)
    TIMEFRAME = "1h"        # Data interval (as requested: 1 hour)
    RETRIEVAL_RANGE = "1y"  # How far back to go (1 year)
    FILE_NAME = "trading_data.csv"
    
    download_trading_data(SYMBOL, TIMEFRAME, RETRIEVAL_RANGE, FILE_NAME)