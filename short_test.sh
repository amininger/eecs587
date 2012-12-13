#!/bin/sh
#PBS -S /bin/sh
#PBS -N AM-short
#PBS -l feature=xeon5650,qos=flux,nodes=3:ppn=12,walltime=15:00,mem=4gb
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

#mpirun -np 1 launch ../../agents/qt_short.soar

echo -e "\n\n=================="
echo "solitary soar test"
cd ~/eecs587/solitary_soar/out

#mpirun -np 1 launch ../../agents/qt_short.soar

echo -e "\n\n================="
echo "parallel soar test"

cd ~/eecs587/soar/out

for use_var in 0 1
do
  for np in 3 10 #  3 4 6 10 18 34  
  do
    echo -e "\n\n-- NP($np) Use_Var($use_var) --"
    mpirun -np $np launch 10 $use_var 1 ../../agents/qt_short.soar
  done
done


