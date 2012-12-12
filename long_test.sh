#!/bin/sh
#PBS -S /bin/sh
#PBS -N AM-long
#PBS -l feature=xeon5650,qos=flux,nodes=3:ppn=12,walltime=03:00:00,mem=8gb
#PBS -M mininger@umich.edu
#PBS -q flux
#PBS -A eecs587f12_flux
#PBS -V
#

echo "I ran on:"
cat $PBS_NODEFILE

source ~/eecs587/source_vars

echo -e "\n\n=================="
echo "original soar test"
cd ~/eecs587/original_soar/out

mpirun -np 1 launch ../../agents/long_test.soar

echo -e "\n\n=================="
echo "solitary soar test"
cd ~/eecs587/solitary_soar/out

mpirun -np 1 launch ../../agents/long_test.soar

echo -e "\n\n================="
echo "parallel soar test"

cd ~/eecs587/soar/out

for use_var in 0 1
do
  for np in 3 4 6 10 18 34
  do
    echo -e "\n\n-- NP($np) Use_Var($use_var) --"
    mpirun -np $np launch 10 $use_var ../../agents/long_test.soar
  done
done


