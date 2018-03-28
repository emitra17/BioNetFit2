#!/bin/bash
# set the number of nodes
#SBATCH --nodes=4

# Force to run on only nodes 300-371, which are all identical
#SBATCH --constraint="nvme:no,baseboard_vendor:HP,cpu_vendor:Intel,cpu_family:broadwell,cpu_model:E5-2695_v4,cpu_base_clock:2.10GHz,numa_nodes:2,clusterondie:no,multithreading:yes,ib:edr,ethernet:1Gb,ssd:no,hdd:no,gpu_count:0"


# Require that we get allocated the whole nodes, so we get comparable runs. 
#SBATCH --exclusive

# set the number of tasks (processes) per node.
#SBATCH --ntasks-per-node=72

# set max wallclock time
#SBATCH --time=10:00:00

# set name of job
#SBATCH --job-name=bnf2

#SBATCH --output=slurm-bnf2-%j.cout


# Run BNF2
# Assumes already compiled, and you set up your node requests above to only get nodes that can use that binary.
time bin/BioNetFit2 -c examples_clean/example0/parabola_ga.conf
