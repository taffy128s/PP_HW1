rm -f output*
pass=1
for i in $(seq -f "%02g" 1 10)
do
  printf "\n\n-----Now testing No.$i...\n"
  time srun -N 1 -n 1 -p batch ./HW1_103062122_basic 0 ./samples/testcase$i output$i
  x=$(diff ./samples/sorted$i output$i)
  time srun -N 1 -n 2 -p batch ./HW1_103062122_basic 0 ./samples/testcase$i output$i
  y=$(diff ./samples/sorted$i output$i)
  time srun -N 1 -n 4 -p batch ./HW1_103062122_basic 0 ./samples/testcase$i output$i
  a=$(diff ./samples/sorted$i output$i)
  time srun -N 1 -n 8 -p batch ./HW1_103062122_basic 0 ./samples/testcase$i output$i
  b=$(diff ./samples/sorted$i output$i)
  if [[ $x == "" ]] && [[ $y == "" ]] && [[ $a == "" ]] && [[ $b == "" ]]
  then 
    pass=$(($pass & 1))
  else 
    pass=$(($pass & 0))
  fi
done

if [ $pass -eq 1 ]
then 
  printf "***********passed!!!***********\n"
else 
  printf "***********not passed!!! GG***********\n"
fi
