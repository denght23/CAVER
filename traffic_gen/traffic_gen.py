import sys
import random
import math
import heapq
from optparse import OptionParser

from custom_rand import CustomRand
import numpy as np

class Flow:
	def __init__(self, src, dst, size, t):
		self.src, self.dst, self.size, self.t = src, dst, size, t
	def __str__(self):
		return "%d %d 3 %d %.9f"%(self.src, self.dst, self.size, self.t)

def translate_bandwidth(b):
	if b == None:
		return None
	if type(b)!=str:
		return None
	if b[-1] == 'G':
		return float(b[:-1])*1e9
	if b[-1] == 'M':
		return float(b[:-1])*1e6
	if b[-1] == 'K':
		return float(b[:-1])*1e3
	return float(b)

def poisson(lam):
	return -math.log(1-random.random())*lam

def generate_normal_integer(mean, std_dev):
    random_float = np.random.normal(mean, std_dev)
    random_int = int(round(random_float))
    return random_int

def get_random_host_by_pod(pod_size, pod_no):
	return random.randint(pod_size * pod_no, pod_size * (pod_no + 1) - 1)

if __name__ == "__main__":
	port = 80
	parser = OptionParser()
	parser.add_option("-c", "--cdf", dest = "cdf_file", help = "the file of the traffic size cdf", default = "Solar2022.txt")
	parser.add_option("-n", "--nhost", dest = "nhost", help = "number of hosts")
	parser.add_option("-l", "--load", dest = "load", help = "the percentage of the traffic load to the network capacity, by default 0.3", default = "0.3")
	parser.add_option("-b", "--bandwidth", dest = "bandwidth", help = "the bandwidth of host link (G/M/K), by default 10G", default = "10G")
	parser.add_option("-t", "--time", dest = "time", help = "the total run time (s), by default 10", default = "10")
	parser.add_option("-o", "--output", dest = "output", help = "the output file", default = "tmp_traffic.txt")
	
	parser.add_option("-m", "--mix", dest = "mix", help = "the ratio of all-to-all", default = "0")
	parser.add_option("-p", "--podsize", dest = "podsize", help = "the pod-size in all-to-all", default = "16")
	
	options,args = parser.parse_args()

	base_t = 2000000000 # 2000000000

	if not options.nhost:
		print("please use -n to enter number of hosts")
		sys.exit(0)
	nhost = int(options.nhost)
	load = float(options.load)
	bandwidth = translate_bandwidth(options.bandwidth)
	time = float(options.time)*1e9 # translates to ns
	output = options.output
	mix = float(options.mix) #all-to-all所占的比例
	podsize = int(options.podsize)
	if bandwidth == None:
		print("bandwidth format incorrect")
		sys.exit(0)

	fileName = options.cdf_file
	file = open(fileName,"r")
	lines = file.readlines()
	# read the cdf, save in cdf as [[x_i, cdf_i] ...]
	cdf = []
	for line in lines:
		x,y = map(float, line.strip().split(' '))
		cdf.append([x,y])

	# create a custom random generator, which takes a cdf, and generate number according to the cdf
	customRand = CustomRand()
	if not customRand.setCdf(cdf):
		print("Error: Not valid cdf")
		sys.exit(0)

	ofile = open(output, "w")

	# generate flows
	avg = customRand.getAvg()
	flows = []
    # 首先，生成random流量
	avg_inter_arrival = 1/(bandwidth*load*(1-mix)/8./avg)*1000000000 
	#n_flow_estimate = int(time / avg_inter_arrival * nhost)
	#n_flow = 0
	host_list = [(base_t + int(poisson(avg_inter_arrival)), i) for i in range(nhost)]# (时间，hostid)
	heapq.heapify(host_list)
	while len(host_list) > 0:
		t,src = host_list[0]
		inter_t = int(poisson(avg_inter_arrival))
		dst = random.randint(0, nhost-1)
		while (dst == src):
			dst = random.randint(0, nhost-1)
		if (t + inter_t > time + base_t):
			heapq.heappop(host_list)
		else:
			size = int(customRand.rand())
			if size <= 0:
				size = 1
			#n_flow += 1
			flows.append(Flow(src, dst, size, t * 1e-9))
			heapq.heapreplace(host_list, (t + inter_t, src))

	print(f'random流量生成完成')

	# 随后，生成all-to-all
	if mix != 0:
		section_size_mean = 200 # 每次all-to-all的流数量平均
		section_size_std = 10	# 每次all-to-all的流数量标准差
		flow_interval_coefficient = 0.2
		flow_interval = avg_inter_arrival * flow_interval_coefficient

		avg_section_interval = 1/(bandwidth*load*mix/8./avg)*1000000000 * section_size_mean / podsize
		#print(f'section_size_mean: {avg_section_interval}')
		#n_flow = 0

		if nhost % podsize != 0:
			print(f"PODSIZE ERROR: nhost{nhost} % podsize{podsize} != 0")
			quit(0)
		n_pod = nhost // podsize
		pod_list = [(base_t + int(poisson(avg_section_interval)), i) for i in range(n_pod)]
		heapq.heapify(pod_list)
		while len(pod_list) > 0:
			t, src_pod = pod_list[0]
			inter_t = int(poisson(avg_section_interval))
			dst_pod = random.randint(0, n_pod-1)
			while dst_pod == src_pod:
				dst_pod = random.randint(0, n_pod - 1)
			if t + inter_t > time + base_t:
				heapq.heappop(pod_list)
			else:
				section_flow_n = generate_normal_integer(section_size_mean, section_size_std)
				cur_time = t
				for _ in range(section_flow_n):
					src_host = get_random_host_by_pod(podsize, src_pod)
					dst_host = get_random_host_by_pod(podsize, dst_pod)
					size = int(customRand.rand())
					if size <= 0:
						size = 1
					flows.append(Flow(src_host, dst_host, size, cur_time * 1e-9))
					cur_time += int(poisson(flow_interval))
				heapq.heapreplace(pod_list, (t + inter_t, src_pod))

	flows.sort(key=lambda x : x.t)
	ofile.write(f"{len(flows)}\n")
	for f in flows:
		ofile.write(f.__str__() + '\n')
	ofile.close()
	#quit(0)
	from matplotlib import pyplot as plt
	times = [flow.t for flow in flows if flow.src == 0]
	
	# 定义时间段的长度
	bin_width = 0.001

	# 生成时间段的边界
	min_time = min(times)
	max_time = max(times)
	bins = np.arange(min_time, max_time + bin_width, bin_width)

	# 使用numpy的histogram来统计每个时间段内的流数量
	counts, bin_edges = np.histogram(times, bins=bins)

	# 绘制直方图
	plt.figure(figsize=(10,6))
	plt.bar(bin_edges[:-1], counts, width=bin_width, edgecolor='black', align='edge')
	plt.xlabel('Time (seconds)')
	plt.ylabel('Flow Count')
	plt.title('Flow Count Distribution Over Time (Bin width = 0.001 seconds)')
	plt.grid(True)
	plt.tight_layout()

	# 展示图表
	plt.savefig(f'flow_count-load{load}-mix{mix}.png')