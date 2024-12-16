import matplotlib.pyplot as plt

def read_throughput_data(filename):
    times = []
    throughputs = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip empty or commented lines
            if not line or line.startswith('#'):
                continue

            cols = line.split()
            # Format: "1s" "0" "470.438"
            # cols[0] = "1s", cols[1] = "0", cols[2] = "470.438"
            # We only want the integer part from "1s" and throughput from cols[2].
            
            # Remove the trailing 's' from the time
            t_str = cols[0].replace('s', '')
            t = float(t_str)
            thr = float(cols[2])
            
            times.append(t)
            throughputs.append(thr)
    return times, throughputs

# Read data from files
time_1, throughput_1 = read_throughput_data('Swift-example-s1-r1-throughput.dat')
time_2, throughput_2 = read_throughput_data('Swift-example-s2-r2-throughput.dat')
time_3, throughput_3 = read_throughput_data('Swift-example-s3-r1-throughput.dat')

# Create a new figure and plot
plt.figure(figsize=(10, 6))

# Plot each flow line
plt.plot(time_1, throughput_1, marker='o', label='S1-R1')
plt.plot(time_2, throughput_2, marker='s', label='S2-R1')
plt.plot(time_3, throughput_3, marker='^', label='S3-R2')

# Add titles and labels
plt.title('Flow Throughput over Time')
plt.xlabel('Time (s)')
plt.ylabel('Throughput (Mbps)')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()
