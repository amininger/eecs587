#!/bin/sh
#PBS -S /bin/sh
#PBS -N AM-soar
#PBS -l feature=xeon5650,qos=flux,nodes=1:ppn=12,walltime=01:00,mem=1gb
#PBS -M mininger@umich.edu
#PBS -q flux
#PBS -A eecs587f12_flux
#PBS -V
#

echo "I ran on:"
cat $PBS_NODEFILE

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:~/eecs587/soar/out

cd ~/eecs587/soar/TestCLI

mpirun -np 4 launch 
mpirun -np 6 launch
mpirun -np 8 launch
mpirun -np 10 launch
mpirun -np 12 launch

