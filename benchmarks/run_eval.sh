#!/bin/bash -x

######################################################################
#  This script runs experiments based on the following               #
#  command line arguments                                            #
#      [R1, R2] : Range over which the read threads are varied       #
#      [W1, W2] : Range over which the write threads are varied      #
#      T : Type of the tree                                          #
#      B : Initial bulk load                                         #
#      N : Number of operations per thread                           #
#      X : Number of operations after which each thread reports      #
#                                                                    #
#  References:                                                       #
#      - http://tuxtweaks.com/2014/05/bash-getopts/                  #
######################################################################

# Set Script Name variable
SCRIPT=`basename ${BASH_SOURCE[0]}`

# Initialize variables to default values.
R1=1
R2=1
W1=1
W2=1
T=1
B=1000000000
N=1000000000
X=100000

# Set fonts for Help.
NORM=`tput sgr0`
BOLD=`tput bold`
REV=`tput smso`

# Help function
function HELP {
  echo -e \\n"Help documentation for ${BOLD}${SCRIPT}.${NORM}"\\n
  echo -e "${REV}Basic usage:${NORM} ${BOLD}$SCRIPT file.ext${NORM}"\\n
  echo "Command line switches are optional. The following switches are recognized."
  echo "${REV}-i${NORM}  --Sets the start value for the number of read threads ${BOLD}i${NORM}. Default is ${BOLD}1${NORM}."
  echo "${REV}-j${NORM}  --Sets the end value for the number of read threads ${BOLD}j${NORM}. Default is ${BOLD}1${NORM}."
  echo "${REV}-c${NORM}  --Sets the start value for the number of write threads ${BOLD}c${NORM}. Default is ${BOLD}1${NORM}."
  echo "${REV}-d${NORM}  --Sets the end value for the number of write threads ${BOLD}d${NORM}. Default is ${BOLD}1${NORM}."
  echo "${REV}-t${NORM}  --Sets the value for tree type ${BOLD}t${NORM}. Default is ${BOLD}olc${NORM}."
  echo "${REV}-b${NORM}  --Sets the value for bulk load limit ${BOLD}b${NORM}. Default is ${BOLD}1000000000${NORM}."
  echo "${REV}-n${NORM}  --Sets the value for number of operations per thread ${BOLD}n${NORM}. Default is ${BOLD}1000000000${NORM}."
  echo "${REV}-x${NORM}  --Sets the value for number of operations to report time for ${BOLD}n${NORM}. Default is ${BOLD}100000${NORM}."
  echo -e "${REV}-h${NORM}  --Displays this help message. No further functions are performed."\\n
  echo -e "Example: ${BOLD}$SCRIPT -r1 10 -r2 20 -w1 10 -w2 20 -t hybrid${NORM}"\\n
  exit 1
}

# Check the number of arguments. If none are passed, print help and exit.
NUMARGS=$#
echo -e \\n"Number of arguments: $NUMARGS"
if [ $NUMARGS -eq 0 ]; then
  HELP
fi

### Start getopts code ###

#Parse command line flags
#If an option should be followed by an argument, it should be followed by a ":".
#Notice there is no ":" after "h". The leading ":" suppresses error messages from
#getopts. This is required to get my unrecognized option code to work.

while getopts :i:j:c:d:t:b:n:x:h FLAG; do
  case $FLAG in
    i)  #set option "i"
      R1=$OPTARG
      echo "-i used: $OPTARG"
      echo "R1 = $R1"
      ;;
    j)  #set option "j"
      R2=$OPTARG
      echo "-j used: $OPTARG"
      echo "R2 = $R2"
      ;;
    c)  #set option "c"
      W1=$OPTARG
      echo "-c used: $OPTARG"
      echo "W1 = $W1"
      ;;
    d)  #set option "d"
      W2=$OPTARG
      echo "-d used: $OPTARG"
      echo "W2 = $W2"
      ;;
    t)  #set option "t"
      T=$OPTARG
      echo "-t used: $OPTARG"
      echo "T = $T"
      ;;
    b)  #set option "b"
      B=$OPTARG
      echo "-b used: $OPTARG"
      echo "B = $B"
      ;;
    n)  #set option "n"
      N=$OPTARG
      echo "-n used: $OPTARG"
      echo "N = $N"
      ;;
    x)  #set option "x"
      X=$OPTARG
      echo "-x used: $OPTARG"
      echo "X = $X"
      ;;
    h)  #show help
      HELP
      ;;
    \?) #unrecognized option - show help
      echo -e \\n"Option -${BOLD}$OPTARG${NORM} not allowed."
      HELP
      ;;
  esac
done

shift $((OPTIND-1))  #This tells getopts to move on to the next argument.

echo $R1
### End getopts code ###

# Set the project root directory
PROJECT_ROOT_DIR=$(dirname $(cd `dirname $0` && pwd))
echo $ROOT_DIR
# Set the results directory
RESULTS_DIR="${PROJECT_ROOT_DIR}/results"
# Create the results directory if it doesn't exist
if [ ! -d "$RESULTS_DIR" ]
then
    sudo mkdir $RESULTS_DIR
fi

# Set the cpu frequency
sudo cpupower frequency-set -g performance

# Loop through for [R1..R2] and [W1..W2] and start the experiment
for i in $(seq $R1 $R2)
do
    for j in $(seq $W1 $W2)
    do
	# Set the directory into which the experiment data will be stored
	EXPT_TIME=`date '+%Y-%m-%d-%H-%M-%S'`
	EXPT_DIR="${RESULTS_DIR}/${EXPT_TIME}_r${i}_w${j}_t${T}_b${B}_n${N}_x${X}"
	sudo mkdir $EXPT_DIR
	echo "Starting experiment $EXPT_DIR"
        sudo su -c "../build/bmk_eval $T $B $i $j $N $X \"$EXPT_DIR/\" > \"${EXPT_DIR}/expt.log\""
	echo "Experiment $EXPT_DIR ended"
    done
done

# End
