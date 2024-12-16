import matplotlib.pyplot as plt

# Initialize lists to store data
time = []
cwnd = []
current_delay = []
target_delay = []

# Read the data from file
with open('cwnd-flow1.dat', 'r') as f:
    for line in f:
        # Skip empty lines
        if not line.strip():
            continue
        # Parse columns
        cols = line.split()
        t = float(cols[0])
        c = float(cols[1])
        cur_d = float(cols[2])
        tar_d = float(cols[3])
        
        time.append(t)
        cwnd.append(c)
        current_delay.append(cur_d)
        target_delay.append(tar_d)

# First figure: CWND over time
plt.figure(figsize=(8, 4))
plt.plot(time, cwnd, marker='o', linestyle='-', color='blue')
plt.xlabel('Time')
plt.ylabel('CWND')
plt.title('CWND over Time')
plt.grid(True)
plt.tight_layout()
plt.show()

# Second figure: Delays over time
plt.figure(figsize=(8, 4))
plt.plot(time, current_delay, marker='.', linestyle='-', color='red', label='End-to-End Delay')
plt.plot(time, target_delay, marker='.', linestyle='-', color='green', label='Target Delay')
plt.xlabel('Time')
plt.ylabel('Delay (s)')
plt.title('Delays over Time')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
