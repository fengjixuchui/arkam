#!/bin/bash

PROJ=$(cd $(dirname $0)/..; pwd)
ARKAM=$PROJ/bin/arkam
SOL=$PROJ/bin/sol
TESTER=$PROJ/lib/tester.sol

cd $PROJ


echo "# ===== test_arkam ====="

./bin/test_arkam || exit 1


echo "# ===== sol ret42 ====="

check_sol () {
  OPTS="$1"
  EXPECT="$2"
  SRC="$3"

  echo -n "$SRC "
  $SOL $OPTS $SRC out/tmp.img || exit 1
  $ARKAM out/tmp.img
  ACTUAL="$?"

  if [ "$ACTUAL" = "$EXPECT" ]; then
      echo "ok"
  else
      echo "ng expected $EXPECT but actual $ACTUAL"
      exit 1
  fi    
}

for src in test/sol_ret42/*.sol
do
  check_sol "--no-corelib" 42 $src
done


echo "# ===== sol err ====="

ERRLOG=out/error_test.log

rm -f $ERRLOG && touch $ERRLOG || exit 1

check_sol_err () {
  SRC="$1"

  echo "" >> $ERRLOG
  echo "===== $SRC =====" >> $ERRLOG
  
  echo -n "$SRC "
  $SOL --no-corelib $SRC out/tmp.img &>> $ERRLOG
  if [ $? != 0 ]; then
      echo "ok"
      return 0
  fi
  
  $ARKAM out/tmp.img &>> $ERRLOG
  if [ $? != 0 ]; then
      echo "ok"
      return 0
  fi
  
  echo "ng error not raised"
  exit 1
}

for src in test/sol_err/*.sol
do
  check_sol_err $src
done


echo "===== sol core library ====="

for src in test/sol_corelib/*.sol
do
  check_sol "" 0 "$TESTER $src"
done
