import sys
import re
from collections import deque

def rolling_average(window_size=20):
    # Queues store the last N values for calculation
    rssi_window = deque(maxlen=window_size)
    lqi_window = deque(maxlen=window_size)
    
    print(f"--- Monitoring Stream (Window Size: {window_size}) ---")
    
    # Process standard input line by line
    for line in sys.stdin:
        # Regex to find RSSI:-XX and LQI:XX
        match = re.search(r"RSSI:(-?\d+)\s+LQI:(\d+)", line)
        if match:
            rssi = int(match.group(1))
            lqi = int(match.group(2))
            
            rssi_window.append(rssi)
            lqi_window.append(lqi)
            
            # Calculate current window averages
            avg_rssi = sum(rssi_window) / len(rssi_window)
            avg_lqi = sum(lqi_window) / len(lqi_window)
            
            # Print update on a single line (overwriting previous output)
            sys.stdout.write(f"\rCurrent RSSI: {rssi:4} (Avg: {avg_rssi:6.2f}) | "
                             f"Current LQI: {lqi:3} (Avg: {avg_lqi:6.2f})   ")
            sys.stdout.flush()

if __name__ == "__main__":
    try:
        rolling_average(window_size=20)
    except KeyboardInterrupt:
        print("\nStream stopped.")
