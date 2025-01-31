import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

file_path = 'data_file.txt'

# Load the data into a pandas DataFrame
df = pd.read_csv(file_path, sep=" ", header=None)

# Rename the columns
df.columns = ['timestamp', 'received_path', 'received_unique_path', 'ideal_min', 'ideal_max', 'ideal_avg']
print(df.shape)
df = df[(df['timestamp'] < 2029000000) & (df['timestamp'] > 2008000000)]
print(df.shape)
df['ratio'] = df['received_unique_path'] / df['ideal_max']

sorted_ratios = np.sort(df['ratio'])
cdf = np.arange(1, len(sorted_ratios) + 1) / len(sorted_ratios)

# Plot the CDF as a line
plt.figure(figsize=(6, 4))
plt.plot(sorted_ratios, cdf, linestyle='-', color='blue')
plt.xlabel('Acceptable Path Detection Ratio', fontsize=22)  # Set x-axis label font size
plt.ylabel('CDF', fontsize=22)  # Set y-axis label font size
#plt.title('CDF of received_unique_path / ideal_max', fontsize=16)  # Set title font size
plt.xlim([-0.01, 3])  # Set x-axis maximum value to 3
plt.ylim([0, 1])
plt.xticks(fontsize=18)
plt.yticks(fontsize=18)
plt.grid(True)
ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.spines['left'].set_linewidth(1.5)
ax.spines['bottom'].set_linewidth(1.5)

#plt.hist(df['ratio'], bins=50, color='blue', alpha=0.7, density=True)
#plt.title('Distribution of received_unique_path / ideal_max')
#plt.xlabel('Ratio (received_unique_path / ideal_max)')
#plt.ylabel('Density')
#plt.grid(True)


plt.savefig('temp.pdf', bbox_inches="tight")
