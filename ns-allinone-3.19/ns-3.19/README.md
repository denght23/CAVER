# NS-3 Simulator for RDMA Network Load Balancing

This is an NS-3 simulator for "Congestion Aware Load Balancing For RDMA with Vector Protocol". We add CAVER's components to  [ConWeave(SIGCOMM'2023)'s NS-3 simulator](https://github.com/conweave-project/conweave-ns3) to simulate RDMA load balancing performance with various algorithms.

We describe how to run this repository either on docker or using your local machine with `ubuntu:20.04`. 


## Run with Docker

#### Docker Engine
For Ubuntu, following the installation guide [here](https://docs.docker.com/engine/install/ubuntu/) and make sure to apply the necessary post-install [steps](https://docs.docker.com/engine/install/linux-postinstall/).
Eventually, you should be able to launch the `hello-world` Docker container without the `sudo` command: `docker run hello-world`.

#### 0. Prerequisites
First, you do all these:

```shell
wget https://www.nsnam.org/releases/ns-allinone-3.19.tar.bz2
tar -xvf ns-allinone-3.19.tar.bz2
cd ns-allinone-3.19
rm -rf ns-3.19
git clone https://github.com/denght23/CAVER.git ns-3.19
```

#### 1. Create a Dockerfile
Here, `ns-allinone-3.19` will be your root directory.

Create a Dockerfile at the root directory with the following:
```shell
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y gnuplot python python3 python3-pip build-essential libgtk-3-0 bzip2 wget git && rm -rf /var/lib/apt/lists/* && pip3 install install numpy matplotlib cycler
WORKDIR /root
```

Then, you do this: 
```shell
docker build -t caver-sim:version1 .
```

Once the container is built, do this from the root directory:
```shell
docker run -it -v $(pwd):/root caver-sim:version1 bash -c "cd ns-3.19; ./waf configure --build-profile=optimized; ./waf"
```

This should build everything necessary for the simulator.

#### 2. Run
One can always just run the container: 
```shell
docker start caver-sim
docker exec -it caver-sim /bin/bash
cd ns-3.19;
./autorun.sh
```

That will run `0.1 second` simulation of the experiment for Figure 2 in the paper.

In the script, you can easily change the network load (e.g., `50%`), runtime (e.g., `0.1s`), or topology (e.g., `leaf-spine`).

This script first calls a traffic generator `./traffic_gen/traffic_gen.py` to create an input trace.
Then, it runs NS-3 simulation script `./scratch/network-load-balance.cc`.

To plot the FCT graph, see below or refer to the script `./analysis/plot_fct.py`.


#### 3. Plot
You can easily plot the results using the following command:
```shell
python3 ./analysis/plot_fct.py
python3 ./analysis/plot_queue.py
python3 ./analysis/plot_uplink.py
```

See below for details of output results.
 
##### Output
As well as above figures, other results are located at `./mix/output`, such as uplink usage (Figure 14), queue number usage per port (Figure 15), etc.

* At `./mix/output`, several raw data is stored such as 
  * Flow Completion Time (`XXX_out_fct.txt`), - Figure 12, 13
  * PFC generation (`XXX_out_pfc.txt`), 
  * Uplink's utility (`XXX_out_uplink.txt`), - Figure 14
  * Number of connections (`XXX_out_conn.txt`), 
  * Congestion Notification Packet (`XXX_out_cnp.txt`).
  * CDF of number of queues usage per egress port (`XXX_out_voq_per_dst_cdf.txt`). - Figure 15 
  * CDF of total queue memory overhead per switch (`XXX_out_voq_cdf.txt`). - Figure 16
  
* Each run of simulation creates a repository in `./mix/output` with simulation ID (10-digit number).
* Inside the folder, you can check the simulation config `config.txt` and output log `config.log`. 
* The history of simulations will be recorded in `./mix/.history`. 

##### Topology
To evaluate on fat-tree (K=8) topology, you can simply change the `TOPOLOGY` variable in `autorun.sh` to `fat_k8_100G_OS2`:
```shell
TOPOLOGY="leaf_spine_128_100G_OS2" # or, fat_k8_100G_OS2
```

##### Clean up
To clean all data of previous simulation results, you can run the command:
```shell
./cleanup.sh
```

#### CAVER Parameters
We include CAVER's parameter values into `./run.py` based on flow control model and topology.  


### Simulator Structure
Most implementations of network load balancing are located in the directory `./src/point-to-point/model`.

* `switch-node.h/cc`: Switching logic that includes a default multi-path routing protocol (e.g., ECMP) and DRILL.
* `switch-mmu.h/cc`: Ingress/egress admission control and PFC.
* `conga-routing.h/cc`: Conga routing protocol.
* `letflow-routing.h/cc`: Letflow routing protocol.
* `conweave-routing.h/cc`: ConWeave routing protocol.
* `conweave-voq.h/cc`: ConWeave in-network reordering buffer.
* `settings.h/cc`: Global variables for logging and debugging.
* `rdma-hw.h/cc`: RDMA-enable NIC behavior model.

<b> RNIC behavior model to out-of-order packet arrival </b>
As disussed in the paper, we observe that RNIC reacts to even a single out-of-order packet sensitively by sending CNP packet.
However, existing RDMA-NS3 simulator (HPCC, DCQCN, TLT-RDMA, etc) did not account for this.
In this simulator, we implemented that behavior in `rdma-hw.cc`.


## Citation
If you find this repository useful in your research, please consider citing:
```
@inproceedings{song2023conweave,
  title={Network Load Balancing with In-network Reordering Support for RDMA},
  author={Song, Cha Hwan and Khooi, Xin Zhe and Joshi, Raj and Choi, Inho and Li, Jialin and Chan, Mun Choon},
  booktitle={Proceedings of SIGCOMM},
  year={2023}
}
```

## Credit
This code repository is based on [https://github.com/alibaba-edu/High-Precision-Congestion-Control](https://github.com/alibaba-edu/High-Precision-Congestion-Control) for Mellanox Connect-X based RDMA-enabled NIC implementation, and [https://github.com/kaist-ina/ns3-tlt-rdma-public.git](https://github.com/kaist-ina/ns3-tlt-rdma-public.git) for Broadcom switch's shared buffer model and IRN implementation.

```
MIT License

Copyright (c) 2023 National University of Singapore

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
